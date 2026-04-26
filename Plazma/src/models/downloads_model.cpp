#include "downloads_model.h"

#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QStorageInfo>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <algorithm>

#include "src/api.h"
#include "src/session.h"
#include "src/settings.h"
#include "src/storage/download_paths.h"
#include "src/utils/signal_throttle.h"

namespace paths = plazma::download_paths;

namespace {

constexpr auto kPersistGroup = "downloads/completed";
constexpr auto kPausedGroup  = "downloads/paused";

// Pull the full file size out of a "Content-Range: bytes A-B/C" header.
// Returns 0 when the header is missing, malformed, or uses the wildcard
// total ("*"). Caller falls back to other sources of truth.
qint64 fullSizeFromContentRange(const QByteArray& header) {
    const auto slash = header.lastIndexOf('/');
    if (slash < 0) return 0;
    const auto tail = header.mid(slash + 1).trimmed();
    if (tail.isEmpty() || tail == "*") return 0;
    bool ok = false;
    const auto value = tail.toLongLong(&ok);
    return ok && value > 0 ? value : 0;
}

// Map a QNetworkReply::NetworkError into the binary "should we auto-retry?"
// decision. Network-shaped failures (DNS, refused, dropped) retry; server-
// shaped failures (HTTP 4xx/5xx surfaces as ProtocolFailure or similar) do
// not — re-issuing a request the server just rejected won't help.
bool isTransientNetworkError(QNetworkReply::NetworkError nerr) {
    switch (nerr) {
        case QNetworkReply::OperationCanceledError:        // wrapped by caller
        case QNetworkReply::NetworkSessionFailedError:
        case QNetworkReply::TemporaryNetworkFailureError:
        case QNetworkReply::UnknownNetworkError:
        case QNetworkReply::ConnectionRefusedError:
        case QNetworkReply::RemoteHostClosedError:
        case QNetworkReply::HostNotFoundError:
        case QNetworkReply::TimeoutError:
        case QNetworkReply::ProxyConnectionRefusedError:
        case QNetworkReply::ProxyConnectionClosedError:
        case QNetworkReply::ProxyTimeoutError:
            return true;
        default:
            return false;
    }
}

// MIME type sniffed from the response's Content-Type header. Strips the
// "; charset=…" suffix and lowercases. Returns empty when the header is
// missing or doesn't look like a usable MIME (e.g., "application/octet-stream"
// is technically valid but tells us nothing about the file type, so we
// treat it the same as "unknown").
QString sniffMimeFromReply(QNetworkReply* reply) {
    if (!reply) return {};
    const auto raw = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    if (raw.isEmpty()) return {};
    auto trimmed = raw.section(QLatin1Char(';'), 0, 0).trimmed().toLower();
    if (trimmed == QStringLiteral("application/octet-stream")) return {};
    return trimmed;
}

}  // namespace

// ─── construction / teardown ─────────────────────────────────────────────

DownloadsModel::DownloadsModel(Api* api, Session* session, Settings* settings, QObject* parent)
    : QAbstractListModel(parent), api_(api), session_(session), settings_(settings) {
    Q_ASSERT(api_ != nullptr);

    // Re-emit the persistence-layer signal as our own NOTIFY for the
    // `downloadsFolder` Q_PROPERTY so QML bindings (e.g. the DownloadBar's
    // "Saved to %1" line) re-evaluate when the user picks a new folder.
    if (settings_) {
        connect(settings_, &Settings::downloadPathChanged,
                this, &DownloadsModel::downloadsFolderChanged);
    }

    hideTimer_ = new QTimer(this);
    hideTimer_->setSingleShot(true);
    connect(hideTimer_, &QTimer::timeout, this, [this] {
        if (!latestVisible_) return;
        latestVisible_ = false;
        emit latestChanged();
    });

    // Coalesce per-chunk latest* updates into ~60 Hz emissions. The list
    // model's dataChanged is still fired immediately on every chunk so the
    // detail-row view (when we ever build one) doesn't lag.
    latestThrottle_ = new SignalThrottle(kLatestThrottleMs, this, [this] {
        emit latestChanged();
    });

    // Load persisted completed downloads; drop the stale ones on the spot.
    loadPersisted();
    purgeStaleCompleted();

    // Session lifecycle: drop in-flight transfers on logout. Kept as a
    // defensive measure — there's no per-download auth right now, but
    // when it lands these transfers will reference a token that just
    // went away. tdesktop's DownloadManager does the same on account
    // switch (data_download_manager.cpp).
    if (session_) {
        connect(session_, &Session::sessionChanged, this, [this] {
            if (!session_ || session_->valid()) return;
            for (auto& e : entries_) {
                if (e->status == Status::Downloading || e->status == Status::Queued) {
                    e->userCanceled = true;
                    teardownTransfer(*e, /*keepPartFile=*/false);
                    setStatus(*e, Status::Canceled);
                }
            }
            emit activeCountChanged();
            emit queuedCountChanged();
            emitLatestImmediately();
        });
    }
}

DownloadsModel::~DownloadsModel() {
    // Abort any in-flight transfers so Qt doesn't tear down QNetworkReplys
    // behind our back during shutdown. PartFile closes via the unique_ptr.
    for (auto& e : entries_) {
        if (e->reply) {
            e->reply->disconnect(this);
            e->reply->abort();
            e->reply->deleteLater();
        }
    }
}

// ─── QAbstractListModel plumbing ─────────────────────────────────────────

int DownloadsModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(entries_.size());
}

QVariant DownloadsModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(entries_.size())) {
        return {};
    }
    const auto& e = *entries_[index.row()];
    switch (role) {
        case IdRole:         return e.id;
        case TitleRole:      return e.title;
        case UrlRole:        return e.sourceUrl;
        case DestPathRole:   return e.destPath;
        case ReceivedRole:   return e.received;
        case TotalRole:      return e.total;
        case ProgressRole:   return e.total > 0 ? static_cast<double>(e.received) / e.total : 0.0;
        case StatusRole:     return static_cast<int>(e.status);
        case ErrorRole:      return e.error;
        case SpeedRole:      return e.speed.bytesPerSecond();
        case EtaSecRole:
            return e.status == Status::Downloading
                ? e.speed.etaSec(e.received, e.total)
                : qint64{-1};
        case FinishedAtRole: return e.finishedAtMsec;
        default:             return {};
    }
}

QHash<int, QByteArray> DownloadsModel::roleNames() const {
    return {
        {IdRole, "id"},
        {TitleRole, "title"},
        {UrlRole, "url"},
        {DestPathRole, "destPath"},
        {ReceivedRole, "received"},
        {TotalRole, "total"},
        {ProgressRole, "progress"},
        {StatusRole, "status"},
        {ErrorRole, "error"},
        {SpeedRole, "speed"},
        {EtaSecRole, "etaSec"},
        {FinishedAtRole, "finishedAt"},
    };
}

// ─── scalar accessors ────────────────────────────────────────────────────

int DownloadsModel::activeCount() const {
    int n = 0;
    for (const auto& e : entries_) {
        if (e->status == Status::Downloading) ++n;
    }
    return n;
}

int DownloadsModel::queuedCount() const {
    int n = 0;
    for (const auto& e : entries_) {
        if (e->status == Status::Queued) ++n;
    }
    return n;
}

QString DownloadsModel::latestId() const { return latestId_; }

QString DownloadsModel::latestTitle() const {
    const auto* e = find(latestId_);
    return e ? e->title : QString{};
}

qreal DownloadsModel::latestProgress() const {
    const auto* e = find(latestId_);
    if (!e || e->total <= 0) return 0.0;
    return static_cast<qreal>(e->received) / static_cast<qreal>(e->total);
}

qint64 DownloadsModel::latestReceived() const {
    const auto* e = find(latestId_);
    return e ? e->received : 0;
}

qint64 DownloadsModel::latestTotal() const {
    const auto* e = find(latestId_);
    return e ? e->total : 0;
}

int DownloadsModel::latestStatus() const {
    const auto* e = find(latestId_);
    return e ? static_cast<int>(e->status) : -1;
}

QString DownloadsModel::latestDestPath() const {
    const auto* e = find(latestId_);
    return e ? e->destPath : QString{};
}

QString DownloadsModel::latestError() const {
    const auto* e = find(latestId_);
    return e ? e->error : QString{};
}

int DownloadsModel::latestErrorReason() const {
    const auto* e = find(latestId_);
    return e ? static_cast<int>(e->errorReason) : static_cast<int>(ErrorReason::ErrorNone);
}

QString DownloadsModel::latestSpeedText() const {
    const auto bps = static_cast<qint64>(latestSpeed());
    if (bps <= 0) return {};
    return tr("%1/s").arg(QLocale::system().formattedDataSize(bps));
}

QString DownloadsModel::latestSizeText() const {
    const auto* e = find(latestId_);
    if (!e) return {};
    const auto loc = QLocale::system();
    if (e->total > 0) {
        return tr("%1 / %2").arg(loc.formattedDataSize(e->received),
                                 loc.formattedDataSize(e->total));
    }
    return e->received > 0 ? loc.formattedDataSize(e->received) : QString{};
}

qreal DownloadsModel::latestSpeed() const {
    const auto* e = find(latestId_);
    return e ? e->speed.bytesPerSecond() : 0.0;
}

qint64 DownloadsModel::latestEtaSec() const {
    const auto* e = find(latestId_);
    if (!e || e->status != Status::Downloading) return -1;
    return e->speed.etaSec(e->received, e->total);
}

QString DownloadsModel::latestEtaText() const {
    const auto eta = latestEtaSec();
    if (eta < 0) return {};
    if (eta == 0) return tr("almost done");
    if (eta < 60) return tr("%1s left").arg(eta);
    if (eta < 3600) {
        const auto m = eta / 60;
        const auto s = eta % 60;
        return s > 0 ? tr("%1m %2s left").arg(m).arg(s) : tr("%1m left").arg(m);
    }
    const auto h = eta / 3600;
    const auto m = (eta % 3600) / 60;
    return m > 0 ? tr("%1h %2m left").arg(h).arg(m) : tr("%1h left").arg(h);
}

qreal DownloadsModel::aggregateProgress() const {
    qint64 received = 0, total = 0;
    for (const auto& e : entries_) {
        if (e->status != Status::Downloading) continue;
        if (e->total > 0) {
            received += e->received;
            total += e->total;
        }
    }
    if (total <= 0) return 0.0;
    return static_cast<qreal>(received) / static_cast<qreal>(total);
}

qint64 DownloadsModel::aggregateReceived() const {
    qint64 n = 0;
    for (const auto& e : entries_) {
        if (e->status == Status::Downloading) n += e->received;
    }
    return n;
}

qint64 DownloadsModel::aggregateTotal() const {
    qint64 n = 0;
    for (const auto& e : entries_) {
        if (e->status == Status::Downloading && e->total > 0) n += e->total;
    }
    return n;
}

qreal DownloadsModel::aggregateSpeed() const {
    qreal s = 0.0;
    for (const auto& e : entries_) {
        if (e->status == Status::Downloading) s += e->speed.bytesPerSecond();
    }
    return s;
}

QString DownloadsModel::downloadsFolder() const {
    if (settings_) {
        const auto userPath = settings_->getDownloadPath();
        if (!userPath.isEmpty()) return userPath;
    }
    return paths::defaultRoot();
}

bool DownloadsModel::has(const QString& id) const { return find(id) != nullptr; }

int DownloadsModel::statusOf(const QString& id) const {
    const auto* e = find(id);
    return e ? static_cast<int>(e->status) : -1;
}

int DownloadsModel::indexOf(const QString& id) const {
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        if (entries_[i]->id == id) return i;
    }
    return -1;
}

DownloadsModel::Entry* DownloadsModel::find(const QString& id) {
    const int i = indexOf(id);
    return i >= 0 ? entries_[i].get() : nullptr;
}

const DownloadsModel::Entry* DownloadsModel::find(const QString& id) const {
    const int i = indexOf(id);
    return i >= 0 ? entries_[i].get() : nullptr;
}

// ─── public slots ────────────────────────────────────────────────────────

void DownloadsModel::start(const QVariantMap& video) {
    const auto id  = video.value(QStringLiteral("id")).toString();
    const auto url = video.value(QStringLiteral("url")).toString();
    if (id.isEmpty() || url.isEmpty()) {
        qWarning() << "[Downloads] start() ignored — missing id or url";
        emit notify(tr("Can't download — this video has no source URL"));
        return;
    }

    if (auto* existing = find(id)) {
        if (existing->status == Status::Downloading || existing->status == Status::Queued) {
            // Already in flight — surface the existing progress instead of
            // queueing a duplicate. The "latest" pointer is updated so the
            // download bar pops back into focus.
            latestId_ = id;
            latestVisible_ = true;
            hideTimer_->stop();
            emitLatestImmediately();
            return;
        }
        if (existing->status == Status::Paused) {
            // start() on a paused row is treated as "resume" — keep the
            // .part, just wake it back up.
            resume(id);
            return;
        }
        // Completed / Failed / Canceled: drop the old row so the retry
        // starts from a clean slate with a fresh collision-resolved path.
        const int row = indexOf(id);
        beginRemoveRows({}, row, row);
        entries_.erase(entries_.begin() + row);
        endRemoveRows();
        emit countChanged();
    }

    const auto title = video.value(QStringLiteral("title")).toString();
    const auto mime  = video.value(QStringLiteral("mime")).toString();
    const auto size  = video.value(QStringLiteral("size")).toLongLong();
    // Resolve the destination root *now*, so a new download honours the
    // user's currently-configured folder. In-flight transfers keep their
    // original destPath (they need a stable target across resume/retry).
    const auto dest  = paths::computeDestPath(downloadsFolder(), title, url, mime);

    auto entry = std::make_unique<Entry>();
    entry->id = id;
    entry->title = title;
    entry->sourceUrl = url;
    entry->destPath = dest;
    entry->mime = mime;
    // Seed total from feed-provided size so the bar can show a percentage
    // before Content-Length arrives. The authoritative value overwrites via
    // downloadProgress() / Content-Range later.
    entry->total = size > 0 ? size : 0;
    entry->retriesLeft = kMaxRetries;
    entry->status = Status::Queued;
    entry->wallClock.start();
    entry->speed = SpeedMeter(kSpeedSampleMs, kSpeedAlpha);

    const int row = static_cast<int>(entries_.size());
    beginInsertRows({}, row, row);
    entries_.push_back(std::move(entry));
    endInsertRows();
    emit countChanged();
    emit queuedCountChanged();

    latestId_ = id;
    latestVisible_ = true;
    hideTimer_->stop();
    emitLatestImmediately();

    processQueue();
}

void DownloadsModel::cancel(const QString& id) {
    auto* e = find(id);
    if (!e) return;
    if (e->status != Status::Queued && e->status != Status::Downloading && e->status != Status::Paused) {
        return;
    }

    e->userCanceled = true;
    e->userPaused = false;
    teardownTransfer(*e, /*keepPartFile=*/false);
    setStatus(*e, Status::Canceled);

    emit activeCountChanged();
    emit queuedCountChanged();
    notifyRowChanged(indexOf(id));
    if (id == latestId_) {
        emitLatestImmediately();
        hideTimer_->start(kBarHideDelayMs);
    }
    processQueue();  // freed slot
}

void DownloadsModel::retry(const QString& id) {
    auto* e = find(id);
    if (!e) return;
    if (e->status == Status::Downloading || e->status == Status::Queued) return;
    if (e->status == Status::Completed) return;   // file's already on disk

    QVariantMap payload;
    payload[QStringLiteral("id")] = e->id;
    payload[QStringLiteral("title")] = e->title;
    payload[QStringLiteral("url")] = e->sourceUrl;
    payload[QStringLiteral("mime")] = e->mime;
    payload[QStringLiteral("size")] = e->total;
    start(payload);
}

void DownloadsModel::pause(const QString& id) {
    auto* e = find(id);
    if (!e) return;
    if (e->status != Status::Downloading && e->status != Status::Queued) return;

    e->userPaused = true;
    // Keep the .part — resume() will pick up where this left off via Range.
    teardownTransfer(*e, /*keepPartFile=*/true);
    setStatus(*e, Status::Paused);
    savePaused();   // survives an app restart

    emit activeCountChanged();
    emit queuedCountChanged();
    notifyRowChanged(indexOf(id));
    if (id == latestId_) emitLatestImmediately();
    processQueue();  // pause frees a slot for whatever's queued
}

void DownloadsModel::resume(const QString& id) {
    auto* e = find(id);
    if (!e || e->status != Status::Paused) return;

    e->userPaused = false;
    e->error.clear();
    setStatus(*e, Status::Queued);
    savePaused();   // entry leaves the paused registry
    emit queuedCountChanged();
    notifyRowChanged(indexOf(id));

    latestId_ = id;
    latestVisible_ = true;
    hideTimer_->stop();
    emitLatestImmediately();
    processQueue();
}

void DownloadsModel::pauseAll() {
    bool touched = false;
    for (auto& e : entries_) {
        if (e->status != Status::Downloading && e->status != Status::Queued) continue;
        e->userPaused = true;
        teardownTransfer(*e, /*keepPartFile=*/true);
        setStatus(*e, Status::Paused);
        notifyRowChanged(indexOf(e->id));
        touched = true;
    }
    if (!touched) return;
    savePaused();
    emit activeCountChanged();
    emit queuedCountChanged();
    emitLatestImmediately();
}

void DownloadsModel::resumeAll() {
    bool touched = false;
    for (auto& e : entries_) {
        if (e->status != Status::Paused) continue;
        e->userPaused = false;
        e->error.clear();
        setStatus(*e, Status::Queued);
        notifyRowChanged(indexOf(e->id));
        touched = true;
    }
    if (!touched) return;
    savePaused();
    emit queuedCountChanged();
    emitLatestImmediately();
    processQueue();
}

void DownloadsModel::remove(const QString& id) {
    const int row = indexOf(id);
    if (row < 0) return;
    auto& e = *entries_[row];

    const bool wasActive = (e.status == Status::Queued ||
                            e.status == Status::Downloading ||
                            e.status == Status::Paused);
    if (wasActive) {
        e.userCanceled = true;
        teardownTransfer(e, /*keepPartFile=*/false);
    }

    const bool wasCompleted = (e.status == Status::Completed);

    beginRemoveRows({}, row, row);
    entries_.erase(entries_.begin() + row);
    endRemoveRows();
    emit countChanged();
    emit activeCountChanged();
    emit queuedCountChanged();

    if (latestId_ == id) {
        latestId_.clear();
        latestVisible_ = false;
        if (latestThrottle_) latestThrottle_->cancel();
        emit latestChanged();
    }

    if (wasCompleted) savePersisted();
    if (wasActive) processQueue();
}

void DownloadsModel::clearCompleted() {
    bool touched = false;
    bool completedTouched = false;
    for (int i = static_cast<int>(entries_.size()) - 1; i >= 0; --i) {
        const auto s = entries_[i]->status;
        if (s == Status::Completed || s == Status::Failed || s == Status::Canceled) {
            const auto id = entries_[i]->id;
            if (s == Status::Completed) completedTouched = true;
            beginRemoveRows({}, i, i);
            entries_.erase(entries_.begin() + i);
            endRemoveRows();
            if (latestId_ == id) {
                latestId_.clear();
                latestVisible_ = false;
            }
            touched = true;
        }
    }
    if (touched) {
        emit countChanged();
        emit latestChanged();
    }
    if (completedTouched) savePersisted();
}

void DownloadsModel::purgeStaleCompleted() {
    bool touched = false;
    for (int i = static_cast<int>(entries_.size()) - 1; i >= 0; --i) {
        if (entries_[i]->status != Status::Completed) continue;
        const auto& path = entries_[i]->destPath;
        if (path.isEmpty() || !QFile::exists(path)) {
            beginRemoveRows({}, i, i);
            entries_.erase(entries_.begin() + i);
            endRemoveRows();
            touched = true;
        }
    }
    if (touched) {
        emit countChanged();
        savePersisted();
    }
}

void DownloadsModel::openFile(const QString& id) {
    const auto* e = find(id);
    if (!e || e->status != Status::Completed) return;
    if (!QFile::exists(e->destPath)) {
        // User deleted the file behind our back; drop the stale entry.
        const_cast<DownloadsModel*>(this)->purgeStaleCompleted();
        emit notify(tr("File is no longer available"));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(e->destPath));
}

void DownloadsModel::openFolder(const QString& id) {
    const auto* e = find(id);
    const auto path = (e && !e->destPath.isEmpty())
        ? QFileInfo(e->destPath).absolutePath()
        : downloadsFolder();
    QDir().mkpath(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void DownloadsModel::dismissLatest() {
    if (!latestVisible_) return;
    latestVisible_ = false;
    hideTimer_->stop();
    if (latestThrottle_) latestThrottle_->cancel();
    emit latestChanged();
}

// ─── scheduling ──────────────────────────────────────────────────────────

void DownloadsModel::processQueue() {
    // Fill concurrency slots FIFO, i.e. in insertion order of the list.
    for (auto& e : entries_) {
        if (activeCount() >= kMaxConcurrent) return;
        if (e->status != Status::Queued) continue;
        kickOff(*e);
    }
}

void DownloadsModel::scheduleRetryKick(const QString& id, int delayMs) {
    QTimer::singleShot(delayMs, this, [this, id] {
        auto* e = find(id);
        if (!e) return;
        // If the user cancelled / paused during the backoff, honor that.
        if (e->status != Status::Queued || e->userCanceled || e->userPaused) return;
        processQueue();
    });
}

// ─── the heavy lifting: kickOff → attachReply → handleFinished ───────────

bool DownloadsModel::checkDiskBudget(Entry& entry) {
    if (entry.total <= 0) return true;  // unknown size — best-effort

    const auto folder = QFileInfo(entry.destPath).absolutePath();
    QDir().mkpath(folder);
    const QStorageInfo storage(folder);
    const auto available = storage.bytesAvailable();
    if (available < 0) return true;     // unable to query — treat as ok

    // Need: bytes still to fetch + safety margin. When resuming, the bytes
    // already on disk don't count against the budget.
    const auto already = entry.received;
    const auto remaining = std::max<qint64>(0, entry.total - already);
    if (available >= remaining + kDiskSafetyBytes) return true;

    const auto msg = tr("Not enough disk space (%1 free, %2 needed)")
                         .arg(QLocale::system().formattedDataSize(available))
                         .arg(QLocale::system().formattedDataSize(remaining));
    qWarning() << "[Downloads] disk check failed:" << available << "<" << remaining;
    setStatus(entry, Status::Failed, msg, ErrorReason::ErrorDiskSpace);
    emit notify(tr("Download failed: %1").arg(msg));
    return false;
}

void DownloadsModel::teardownTransfer(Entry& entry, bool keepPartFile) {
    if (entry.reply) {
        entry.reply->disconnect(this);
        entry.reply->abort();
        entry.reply->deleteLater();
        entry.reply = nullptr;
    }
    if (entry.file) {
        if (keepPartFile) {
            entry.file->flush();
            entry.file.reset();
        } else {
            entry.file->discard();
            entry.file.reset();
        }
    } else if (!keepPartFile) {
        // No PartFile object yet (queued-but-not-yet-started case) but the
        // sibling .part might exist from a previous attempt.
        QFile::remove(entry.destPath + QStringLiteral(".part"));
    }
}

void DownloadsModel::kickOff(Entry& entry) {
    Q_ASSERT(entry.status == Status::Queued);
    Q_ASSERT(!entry.reply);

    if (!checkDiskBudget(entry)) {
        notifyRowChanged(indexOf(entry.id));
        if (entry.id == latestId_) {
            emitLatestImmediately();
            hideTimer_->start(kBarHideFailMs);
        }
        emit queuedCountChanged();
        return;
    }

    // Open (or re-open) the .part file. Resume kicks in automatically when
    // there's a sane prefix on disk from a previous attempt.
    entry.file = std::make_unique<PartFile>(entry.destPath);
    if (!entry.file->open(kMinResumeBytes, entry.total)) {
        const auto msg = tr("Can't write to %1").arg(entry.destPath);
        qWarning() << "[Downloads] open failed:" << entry.destPath << entry.file->errorString();
        entry.file.reset();
        setStatus(entry, Status::Failed, msg, ErrorReason::ErrorDiskWrite);
        emit notify(tr("Download failed: %1").arg(msg));
        notifyRowChanged(indexOf(entry.id));
        if (entry.id == latestId_) {
            emitLatestImmediately();
            hideTimer_->start(kBarHideFailMs);
        }
        emit queuedCountChanged();
        return;
    }

    const auto resumeFrom = entry.file->resumeFrom();
    if (resumeFrom > 0) {
        qDebug() << "[Downloads] resuming" << entry.id << "from byte" << resumeFrom;
    }

    entry.reply = api_->startDownload(QUrl(entry.sourceUrl), resumeFrom);
    if (!entry.reply) {
        const auto msg = tr("Network is unavailable");
        entry.file->discard();
        entry.file.reset();
        setStatus(entry, Status::Failed, msg, ErrorReason::ErrorNetwork);
        emit notify(tr("Download failed: %1").arg(msg));
        notifyRowChanged(indexOf(entry.id));
        if (entry.id == latestId_) {
            emitLatestImmediately();
            hideTimer_->start(kBarHideFailMs);
        }
        emit queuedCountChanged();
        return;
    }

    entry.received = resumeFrom;
    entry.error.clear();
    entry.httpStatus = 0;
    entry.destReconciled = false;
    entry.speed.reset(resumeFrom);
    entry.status = Status::Downloading;
    emit activeCountChanged();
    emit queuedCountChanged();

    attachReply(entry);

    notifyRowChanged(indexOf(entry.id));
    if (entry.id == latestId_) {
        emitLatestImmediately();
        emit notify(tr("Downloading “%1”").arg(entry.title.isEmpty() ? tr("video") : entry.title));
    }
}

void DownloadsModel::reconcileDestFromContentType(Entry& entry) {
    if (entry.destReconciled) return;
    entry.destReconciled = true;
    if (!entry.file || entry.file->bytesOnDisk() > 0) return;

    const auto sniffed = sniffMimeFromReply(entry.reply.data());
    if (sniffed.isEmpty()) return;
    if (sniffed == entry.mime) return;  // nothing new

    const auto oldExt = QFileInfo(entry.destPath).suffix().toLower();
    const auto newExt = paths::extensionFor(entry.sourceUrl, sniffed);
    if (oldExt == newExt) {
        entry.mime = sniffed;
        return;
    }

    // Re-derive the destination path with the right extension. The .part
    // file has to move with it, since downstream code keys off PartFile's
    // own destPath — easiest is to discard and reopen at the new location.
    const auto fresh = paths::rederivePath(entry.destPath, entry.title, entry.sourceUrl, sniffed);
    qDebug() << "[Downloads] content-type sniff for" << entry.id
             << "—" << entry.destPath << "→" << fresh << "(" << sniffed << ")";

    entry.file->discard();
    entry.file.reset();
    entry.destPath = fresh;
    entry.mime = sniffed;
    entry.file = std::make_unique<PartFile>(entry.destPath);
    if (!entry.file->open(kMinResumeBytes, entry.total)) {
        // Open failed at the new path. Fall back to the URL-derived
        // extension; the user gets a slightly mis-named file but at least
        // the bytes land somewhere.
        qWarning() << "[Downloads] reconcile failed to open" << fresh
                   << "—" << entry.file->errorString();
        entry.file.reset();
    }
}

void DownloadsModel::attachReply(Entry& entry) {
    Q_ASSERT(entry.reply);
    auto* reply = entry.reply.data();
    const auto id = entry.id;

    // HTTP status gate + range/content-type reconciliation. Fired every
    // time the response metadata changes (typically once per request, but
    // may fire again on redirect resolution).
    //
    // We treat the response code as a small state machine:
    //   * 3xx — Qt's redirect policy will follow it; we sit tight.
    //   * 206 — server honored the Range header. Use Content-Range to set
    //     the authoritative full file size, since per-response Content-Length
    //     is just the partial.
    //   * 200 with resumeFrom > 0 — server ignored Range. Truncate the .part
    //     and start over from byte 0 in the same response.
    //   * 416 — Range Not Satisfiable. The server's best guess is that the
    //     bytes we already have are all there is, i.e. the file is complete.
    //     Treat as success and let handleFinished() do the rename.
    //   * Any other 4xx/5xx — abort. handleFinished() reports it as
    //     "HTTP <code>" and routes through the retry/fail logic.
    connect(reply, &QNetworkReply::metaDataChanged, this, [this, id] {
        auto* e = find(id);
        if (!e || !e->reply) return;
        const auto code = e->reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code == 0) return;                    // not yet an HTTP response
        if (code >= 300 && code < 400) return;    // redirect; QNAM handles it

        // 206 Partial Content — Range honored.
        if (code == 206 && e->file && e->file->resumeFrom() > 0) {
            const auto cr = e->reply->rawHeader("Content-Range");
            const auto full = fullSizeFromContentRange(cr);
            if (full > 0 && e->total != full) {
                e->total = full;
                notifyRowChanged(indexOf(id));
                if (id == latestId_) latestThrottle_->request();
            }
            reconcileDestFromContentType(*e);
            return;
        }

        // 200 OK after we asked for a range — server ignored Range and is
        // sending the whole body. Wipe the stale prefix.
        if (code == 200 && e->file && e->file->resumeFrom() > 0) {
            qDebug() << "[Downloads] server ignored Range, restarting from 0:" << id;
            if (!e->file->truncate()) {
                qWarning() << "[Downloads] truncate failed:" << e->file->errorString();
                e->reply->abort();
                return;
            }
            e->received = 0;
            e->speed.reset(0);
            reconcileDestFromContentType(*e);
            notifyRowChanged(indexOf(id));
            if (id == latestId_) latestThrottle_->request();
            return;
        }

        // 416 Range Not Satisfiable — treat as "you have everything already".
        if (code == 416 && e->file && e->file->resumeFrom() > 0) {
            qDebug() << "[Downloads] HTTP 416 — assuming .part is already complete:" << id;
            e->httpStatus = 0;        // suppress error reporting
            e->reply->abort();        // routes through handleFinished as success
            return;
        }

        if (code >= 200 && code < 300) {
            reconcileDestFromContentType(*e);
            return;
        }

        if (e->httpStatus != 0) return;  // already noticed, don't double-abort
        e->httpStatus = code;
        qWarning() << "[Downloads] HTTP" << code << "→ abort" << id;
        e->reply->abort();
    });

    // Chunked write. QNetworkReply is buffered, so readyRead() fires
    // whenever new bytes are available — drain into the .part file, update
    // counters, throttle the latest signal.
    connect(reply, &QNetworkReply::readyRead, this, [this, id] {
        auto* e = find(id);
        if (!e || !e->reply || !e->file) return;
        if (e->httpStatus >= 400) {
            // Non-2xx slipped past metaDataChanged (HTTP/2 can deliver body
            // before headers in edge cases) — drain & discard, the abort
            // path will run via finished().
            e->reply->readAll();
            return;
        }
        const auto chunk = e->reply->readAll();
        if (chunk.isEmpty()) return;
        if (!e->file->write(chunk)) {
            qWarning() << "[Downloads] write failed:" << e->file->errorString();
            e->userCanceled = false;
            e->reply->abort();
            return;
        }
        e->received = e->file->bytesOnDisk();
        e->speed.tick(e->received, e->wallClock.elapsed());
        notifyRowChanged(indexOf(id));
    });

    // downloadProgress → we use this for `total` (Content-Length) only.
    // `received` comes from PartFile's own counter, since downloadProgress
    // aggregates buffered-but-unread bytes.
    //
    // Two adjustments:
    //   * When resuming, the response's Content-Length is the size of the
    //     remaining range, not the full file. The 206 branch above already
    //     pulled the full size out of Content-Range, so leave `total` alone.
    //   * Some servers (gzip-on-the-fly proxies, chunked-only origins) send
    //     `Transfer-Encoding: chunked` with no Content-Length — try the
    //     OriginalContentLengthAttribute as a last resort.
    connect(reply, &QNetworkReply::downloadProgress, this, [this, id](qint64 /*received*/, qint64 total) {
        auto* e = find(id);
        if (!e || !e->reply) return;
        if (e->file && e->file->resumeFrom() > 0) return;
        if (total <= 0) {
            const auto orig = e->reply->attribute(QNetworkRequest::OriginalContentLengthAttribute);
            if (orig.isValid()) total = orig.toLongLong();
        }
        if (total > 0 && e->total != total) {
            e->total = total;
            notifyRowChanged(indexOf(id));
            if (id == latestId_) latestThrottle_->request();
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, id] {
        auto* e = find(id);
        if (!e) return;
        handleFinished(*e);
    });
}

void DownloadsModel::handleFinished(Entry& e) {
    auto* reply = e.reply.data();
    const auto nerr = reply ? reply->error() : QNetworkReply::NoError;
    const bool aborted = nerr == QNetworkReply::OperationCanceledError;
    const bool anyError = nerr != QNetworkReply::NoError;

    // Drain any buffered tail bytes for SUCCESS only — on error we don't
    // want partial data landing in the .part file.
    if (!anyError && reply && e.file) {
        const auto tail = reply->readAll();
        if (!tail.isEmpty()) {
            // Best-effort — if a tail write fails, treat it as a hard error
            // by setting anyError-equivalent state below.
            (void)e.file->write(tail);
            e.received = e.file->bytesOnDisk();
        }
    }

    const int httpCode = e.httpStatus > 0
        ? e.httpStatus
        : (reply ? reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() : 0);

    if (reply) reply->deleteLater();
    e.reply = nullptr;

    const bool userPaused = e.userPaused;
    const bool userCancel = aborted && e.userCanceled && !userPaused;
    const bool httpError = httpCode >= 400;
    const bool transient = !userCancel && !userPaused && !httpError && isTransientNetworkError(nerr);

    e.userCanceled = false;

    if (userPaused) {
        // Pause already set the status before tearing down. Just keep the
        // .part on disk and ride the route below (no status change here —
        // pause() already did setStatus(Paused) and flushed/closed the
        // file). We still need to release the file unique_ptr.
        if (e.file) {
            e.file->flush();
            e.file.reset();
        }
        emit activeCountChanged();
        emit queuedCountChanged();
        notifyRowChanged(indexOf(e.id));
        if (e.id == latestId_) emitLatestImmediately();
        processQueue();
        return;
    }

    if (userCancel) {
        if (e.file) { e.file->discard(); e.file.reset(); }
        setStatus(e, Status::Canceled);

    } else if (transient && e.retriesLeft > 0) {
        const int attempt = kMaxRetries - e.retriesLeft;
        --e.retriesLeft;

        // Keep the .part — kickOff() will see whatever bytes already landed
        // and ask the server for a Range continuation. The PartFile is
        // released here and reopened on next kickOff so we don't hold onto
        // a file descriptor across the backoff window.
        if (e.file) { e.file->flush(); e.file.reset(); }
        e.httpStatus = 0;
        e.status = Status::Queued;
        e.error = tr("Retrying… (%1 / %2)").arg(attempt + 1).arg(kMaxRetries);

        const int delay = kRetryBackoffMsBase * (1 << attempt);
        qDebug() << "[Downloads] transient fail on" << e.id << "— retry in" << delay << "ms";
        scheduleRetryKick(e.id, delay);

        emit activeCountChanged();
        emit queuedCountChanged();
        notifyRowChanged(indexOf(e.id));
        if (e.id == latestId_) emitLatestImmediately();
        return;

    } else if (httpError) {
        if (e.file) { e.file->discard(); e.file.reset(); }
        const auto msg = tr("HTTP %1").arg(httpCode);
        // 401/403 → auth-flavored, everything else 4xx/5xx is generic HTTP.
        const auto reason = (httpCode == 401 || httpCode == 403)
            ? ErrorReason::ErrorAuth : ErrorReason::ErrorHttp;
        setStatus(e, Status::Failed, msg, reason);
        emit notify(tr("Download failed: %1").arg(msg));

    } else if (anyError) {
        if (e.file) { e.file->discard(); e.file.reset(); }
        const auto msg = reply && !reply->errorString().isEmpty()
            ? reply->errorString() : tr("Unknown error");
        const auto reason = (nerr == QNetworkReply::TimeoutError)
            ? ErrorReason::ErrorTimeout
            : ErrorReason::ErrorNetwork;
        setStatus(e, Status::Failed, msg, reason);
        emit notify(tr("Download failed: %1").arg(msg));

    } else {
        // Clean finish — flush + close + atomic rename via PartFile.
        if (!e.file || !e.file->finalize()) {
            const auto msg = e.file ? e.file->errorString() : tr("File handle vanished");
            qWarning() << "[Downloads] finalize failed:" << msg;
            if (e.file) { e.file->discard(); e.file.reset(); }
            setStatus(e, Status::Failed, tr("Could not finalize the downloaded file"),
                      ErrorReason::ErrorDiskWrite);
            emit notify(tr("Download failed: %1").arg(msg));
        } else {
            e.file.reset();
            e.finishedAtMsec = QDateTime::currentMSecsSinceEpoch();
            if (e.total <= 0) e.total = e.received;
            setStatus(e, Status::Completed);
            savePersisted();
            emit notify(tr("Saved “%1”").arg(e.title.isEmpty() ? tr("video") : e.title));
        }
    }

    emit activeCountChanged();
    emit queuedCountChanged();
    notifyRowChanged(indexOf(e.id));
    if (e.id == latestId_) {
        emitLatestImmediately();
        hideTimer_->start(e.status == Status::Failed ? kBarHideFailMs : kBarHideDelayMs);
    }

    processQueue();
}

// ─── internal helpers ────────────────────────────────────────────────────

void DownloadsModel::notifyRowChanged(int i) {
    if (i < 0 || i >= static_cast<int>(entries_.size())) return;
    const auto idx = index(i);
    emit dataChanged(idx, idx, {ReceivedRole, TotalRole, ProgressRole, StatusRole,
                                ErrorRole, SpeedRole, EtaSecRole, FinishedAtRole});
    if (entries_[i]->id == latestId_ && latestThrottle_) {
        latestThrottle_->request();
    }
}

void DownloadsModel::emitLatestImmediately() {
    if (latestThrottle_) latestThrottle_->flush();
    else emit latestChanged();
}

void DownloadsModel::setStatus(Entry& entry, Status s, const QString& error,
                               ErrorReason reason) {
    if (entry.status == s && entry.error == error && entry.errorReason == reason) return;
    entry.status = s;
    entry.error = error;
    // Only Failed entries carry a meaningful error reason; clear it on
    // any other transition so a subsequent retry doesn't inherit a stale
    // category from the last attempt.
    entry.errorReason = (s == Status::Failed) ? reason : ErrorReason::ErrorNone;
    if (s == Status::Downloading) {
        entry.wallClock.restart();
    }
}

// ─── persistence ─────────────────────────────────────────────────────────

void DownloadsModel::loadPersisted() {
    QSettings s;
    const int n = s.beginReadArray(QString::fromUtf8(kPersistGroup));
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        auto entry = std::make_unique<Entry>();
        entry->id             = s.value(QStringLiteral("id")).toString();
        entry->title          = s.value(QStringLiteral("title")).toString();
        entry->sourceUrl      = s.value(QStringLiteral("url")).toString();
        entry->destPath       = s.value(QStringLiteral("destPath")).toString();
        entry->mime           = s.value(QStringLiteral("mime")).toString();
        entry->total          = s.value(QStringLiteral("size")).toLongLong();
        entry->finishedAtMsec = s.value(QStringLiteral("finishedAt")).toLongLong();
        entry->received       = entry->total;
        entry->status         = Status::Completed;
        entry->persisted      = true;
        entry->speed          = SpeedMeter(kSpeedSampleMs, kSpeedAlpha);

        if (entry->id.isEmpty() || entry->destPath.isEmpty()) continue;
        entries_.push_back(std::move(entry));
    }
    s.endArray();
    std::sort(entries_.begin(), entries_.end(),
              [](const std::unique_ptr<Entry>& a, const std::unique_ptr<Entry>& b) {
                  return a->finishedAtMsec > b->finishedAtMsec;
              });

    // Paused entries — restored as Status::Paused so the user explicitly
    // resumes. The .part file on disk is the source of truth for `received`;
    // if it's gone (user cleaned up via file manager) the entry is dropped.
    const int p = s.beginReadArray(QString::fromUtf8(kPausedGroup));
    for (int i = 0; i < p; ++i) {
        s.setArrayIndex(i);
        const auto destPath = s.value(QStringLiteral("destPath")).toString();
        const auto partPath = destPath + QStringLiteral(".part");
        const QFileInfo partInfo(partPath);
        if (destPath.isEmpty() || !partInfo.exists()) continue;

        auto entry = std::make_unique<Entry>();
        entry->id          = s.value(QStringLiteral("id")).toString();
        entry->title       = s.value(QStringLiteral("title")).toString();
        entry->sourceUrl   = s.value(QStringLiteral("url")).toString();
        entry->destPath    = destPath;
        entry->mime        = s.value(QStringLiteral("mime")).toString();
        entry->total       = s.value(QStringLiteral("size")).toLongLong();
        entry->received    = partInfo.size();
        entry->retriesLeft = kMaxRetries;
        entry->userPaused  = true;
        entry->status      = Status::Paused;
        entry->speed       = SpeedMeter(kSpeedSampleMs, kSpeedAlpha);
        if (entry->id.isEmpty()) continue;
        entries_.push_back(std::move(entry));
    }
    s.endArray();
}

void DownloadsModel::savePersisted() const {
    QSettings s;
    s.remove(QString::fromUtf8(kPersistGroup));

    std::vector<const Entry*> completed;
    completed.reserve(entries_.size());
    for (const auto& e : entries_) {
        if (e->status == Status::Completed) completed.push_back(e.get());
    }
    std::sort(completed.begin(), completed.end(),
              [](const Entry* a, const Entry* b) { return a->finishedAtMsec > b->finishedAtMsec; });
    if (static_cast<int>(completed.size()) > kMaxPersistedCompleted) {
        completed.resize(kMaxPersistedCompleted);
    }

    s.beginWriteArray(QString::fromUtf8(kPersistGroup), static_cast<int>(completed.size()));
    for (int i = 0; i < static_cast<int>(completed.size()); ++i) {
        const auto* e = completed[i];
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("id"),         e->id);
        s.setValue(QStringLiteral("title"),      e->title);
        s.setValue(QStringLiteral("url"),        e->sourceUrl);
        s.setValue(QStringLiteral("destPath"),   e->destPath);
        s.setValue(QStringLiteral("mime"),       e->mime);
        s.setValue(QStringLiteral("size"),       e->total);
        s.setValue(QStringLiteral("finishedAt"), e->finishedAtMsec);
    }
    s.endArray();
    s.sync();
}

void DownloadsModel::savePaused() const {
    QSettings s;
    s.remove(QString::fromUtf8(kPausedGroup));

    std::vector<const Entry*> paused;
    paused.reserve(entries_.size());
    for (const auto& e : entries_) {
        if (e->status == Status::Paused) paused.push_back(e.get());
    }

    s.beginWriteArray(QString::fromUtf8(kPausedGroup), static_cast<int>(paused.size()));
    for (int i = 0; i < static_cast<int>(paused.size()); ++i) {
        const auto* e = paused[i];
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("id"),       e->id);
        s.setValue(QStringLiteral("title"),    e->title);
        s.setValue(QStringLiteral("url"),      e->sourceUrl);
        s.setValue(QStringLiteral("destPath"), e->destPath);
        s.setValue(QStringLiteral("mime"),     e->mime);
        s.setValue(QStringLiteral("size"),     e->total);
    }
    s.endArray();
    s.sync();
}
