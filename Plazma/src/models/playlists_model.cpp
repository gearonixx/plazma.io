#include "playlists_model.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <algorithm>
#include <unordered_set>

#include "../api.h"
#include "../session.h"

namespace {
constexpr int kPreviewThumbs = 4;
}  // namespace

PlaylistsModel::PlaylistsModel(Api* api, Session* session, QObject* parent)
    : QAbstractListModel(parent), api_(api) {
    // Auto-refresh as soon as the user is authenticated. We don't refresh on
    // construction because Api won't have a token yet; the listing endpoint
    // is auth-required (see playlists.md §4) so the call would just 401.
    if (session != nullptr) {
        connect(session, &Session::sessionChanged, this, [this, session]() {
            if (session->valid()) refresh();
        });
        if (session->valid()) refresh();
    }
}

int PlaylistsModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(playlists_.size());
}

QVariant PlaylistsModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(playlists_.size())) {
        return {};
    }
    const auto& p = playlists_[index.row()];
    switch (role) {
        case IdRole:
            return p.id;
        case NameRole:
            return p.name;
        case VideoCountRole:
            return static_cast<int>(p.videos.size());
        case ThumbnailsRole:
            return previewThumbnails(p);
        case FirstThumbRole:
            return firstThumbnail(p);
        case UpdatedAtRole:
            return p.updatedAt;
        default:
            return {};
    }
}

QHash<int, QByteArray> PlaylistsModel::roleNames() const {
    return {
        {IdRole, "id"},
        {NameRole, "name"},
        {VideoCountRole, "videoCount"},
        {ThumbnailsRole, "thumbnails"},
        {FirstThumbRole, "coverThumbnail"},
        {UpdatedAtRole, "updatedAt"},
    };
}

QString PlaylistsModel::currentPlaylistName() const {
    const auto* p = find(currentId_);
    return p ? p->name : QString();
}

QVariantList PlaylistsModel::currentVideos() const {
    const auto* p = find(currentId_);
    if (!p) return {};
    QVariantList out;
    out.reserve(static_cast<int>(p->videos.size()));
    for (const auto& v : p->videos) out.push_back(videoToVariant(v));
    return out;
}

// ── Mutations ────────────────────────────────────────────────────────────────

QString PlaylistsModel::createPlaylist(const QString& name) {
    const QString trimmed = name.trimmed();
    if (!isValidName(trimmed) || isNameTaken(trimmed)) {
        emit notify(tr("Choose a different name"));
        return {};
    }

    Playlist p;
    p.id = makeId();
    p.name = trimmed;
    p.createdAt = isoNowUtc();
    p.updatedAt = p.createdAt;
    const QString newId = p.id;

    playlists_.push_back(std::move(p));
    sortByName();

    beginResetModel();
    endResetModel();
    emit countChanged();

    lastCreatedId_ = newId;
    emit lastCreatedChanged();

    if (api_) {
        api_->createPlaylistRemote(
            newId,
            trimmed,
            [this, newId](const QJsonObject& playlist) {
                // Server may have normalized fields (e.g. trimmed whitespace
                // we missed, server-stamped timestamps). Reconcile just this
                // row so the local mirror is canonical.
                reconcileSingleSummary(playlist);
            },
            [this, newId, trimmed](int code, const QString& error) {
                qWarning() << "[Playlists] create failed; rolling back local row:" << newId << code << error;
                // Roll back optimistic insert and surface the message.
                deletePlaylist(newId);
                emit notify(error.isEmpty() ? tr("Could not create playlist") : error);
            }
        );
    }
    return newId;
}

bool PlaylistsModel::renamePlaylist(const QString& id, const QString& newName) {
    const QString trimmed = newName.trimmed();
    if (!isValidName(trimmed) || isNameTaken(trimmed, id)) return false;

    auto* p = find(id);
    if (!p) return false;
    if (p->name == trimmed) return true;

    const QString prevName = p->name;
    const QString prevUpdatedAt = p->updatedAt;
    p->name = trimmed;
    touch(*p);

    sortByName();
    beginResetModel();
    endResetModel();

    if (id == currentId_) emit currentChanged();

    if (api_) {
        api_->renamePlaylistRemote(
            id,
            trimmed,
            [this, id](const QJsonObject& playlist) { reconcileSingleSummary(playlist); },
            [this, id, prevName, prevUpdatedAt](int code, const QString& error) {
                qWarning() << "[Playlists] rename failed; rolling back:" << id << code << error;
                if (auto* p = find(id)) {
                    p->name = prevName;
                    p->updatedAt = prevUpdatedAt;
                    sortByName();
                    beginResetModel();
                    endResetModel();
                    if (id == currentId_) emit currentChanged();
                }
                emit notify(error.isEmpty() ? tr("Could not rename playlist") : error);
            }
        );
    }
    return true;
}

bool PlaylistsModel::deletePlaylist(const QString& id) {
    const int row = indexOf(id);
    if (row < 0) return false;

    // Snapshot so we can roll back if the server rejects (e.g. transient 5xx
    // before the server side has actually deleted). Cheap enough — playlists
    // are bounded by the per-user cap (see playlists.md §8).
    Playlist snapshot = playlists_[row];

    beginRemoveRows({}, row, row);
    playlists_.erase(playlists_.begin() + row);
    endRemoveRows();
    emit countChanged();

    if (currentId_ == id) {
        currentId_.clear();
        emit currentChanged();
    }

    if (api_) {
        api_->deletePlaylistRemote(
            id,
            /*onSuccess=*/{},
            [this, id, snapshot = std::move(snapshot)](int code, const QString& error) mutable {
                qWarning() << "[Playlists] delete failed; restoring:" << id << code << error;
                playlists_.push_back(std::move(snapshot));
                sortByName();
                beginResetModel();
                endResetModel();
                emit countChanged();
                emit notify(error.isEmpty() ? tr("Could not delete playlist") : error);
            }
        );
    }
    return true;
}

bool PlaylistsModel::addVideoToPlaylist(const QString& playlistId, const QVariantMap& videoMap) {
    auto* p = find(playlistId);
    if (!p) return false;

    Video v = videoFromVariant(videoMap);
    if (v.id.isEmpty() && v.url.isEmpty()) return false;

    // Local dedup. The server is the authority — see playlists.md §3.7,
    // re-add returns 200 with the original `added_at` — but checking
    // locally lets us surface the correct toast without a round trip.
    const auto dupe = [&](const Video& x) {
        if (!v.id.isEmpty()) return x.id == v.id;
        return !x.url.isEmpty() && x.url == v.url;
    };
    if (std::any_of(p->videos.begin(), p->videos.end(), dupe)) {
        emit notify(tr("Already in “%1”").arg(p->name));
        return false;
    }

    if (v.addedAt.isEmpty()) v.addedAt = isoNowUtc();
    p->videos.push_back(v);  // copy — we still need v for the API call
    touch(*p);

    emitRowChanged(indexOf(playlistId));
    if (playlistId == currentId_) emit currentChanged();
    emit notify(tr("Saved to “%1”").arg(p->name));

    if (api_) {
        api_->addPlaylistItem(
            playlistId,
            videoSnapshotForServer(v),
            [this, playlistId](const QJsonObject& itemJson, const QJsonObject& playlistJson) {
                // Server stamps the canonical `added_at`; replace local row
                // so future rendering matches.
                if (auto* p = find(playlistId)) {
                    const auto videoId = itemJson.value("video_id").toString();
                    for (auto& it : p->videos) {
                        if (it.id == videoId) {
                            it = itemFromJson(itemJson);
                            break;
                        }
                    }
                    emitRowChanged(indexOf(playlistId));
                    if (playlistId == currentId_) emit currentChanged();
                }
                if (!playlistJson.isEmpty()) reconcileSingleSummary(playlistJson);
            },
            [this, playlistId, videoId = v.id](int code, const QString& error) {
                qWarning() << "[Playlists] add item failed; rolling back:" << playlistId << videoId << code << error;
                if (auto* p = find(playlistId); p && !videoId.isEmpty()) {
                    p->videos.erase(
                        std::remove_if(p->videos.begin(), p->videos.end(),
                                       [&](const Video& x) { return x.id == videoId; }),
                        p->videos.end()
                    );
                    emitRowChanged(indexOf(playlistId));
                    if (playlistId == currentId_) emit currentChanged();
                }
                emit notify(error.isEmpty() ? tr("Could not save video") : error);
            }
        );
    }
    return true;
}

bool PlaylistsModel::removeVideoFromPlaylist(const QString& playlistId, const QString& videoId) {
    auto* p = find(playlistId);
    if (!p) return false;

    const auto it = std::find_if(p->videos.begin(), p->videos.end(),
                                 [&](const Video& v) { return v.id == videoId; });
    if (it == p->videos.end()) return false;

    Video snapshot = *it;
    p->videos.erase(it);
    touch(*p);

    emitRowChanged(indexOf(playlistId));
    if (playlistId == currentId_) emit currentChanged();

    if (api_) {
        api_->removePlaylistItem(
            playlistId,
            videoId,
            /*onSuccess=*/{},
            [this, playlistId, snapshot = std::move(snapshot)](int code, const QString& error) mutable {
                qWarning() << "[Playlists] remove item failed; restoring:" << playlistId << snapshot.id << code << error;
                if (auto* p = find(playlistId)) {
                    p->videos.push_back(std::move(snapshot));
                    emitRowChanged(indexOf(playlistId));
                    if (playlistId == currentId_) emit currentChanged();
                }
                emit notify(error.isEmpty() ? tr("Could not remove video") : error);
            }
        );
    }
    return true;
}

// ── Read helpers (synchronous, served from local mirror) ────────────────────

bool PlaylistsModel::playlistContains(const QString& playlistId, const QString& videoId) const {
    const auto* p = find(playlistId);
    if (!p || videoId.isEmpty()) return false;
    return std::any_of(p->videos.begin(), p->videos.end(), [&](const Video& v) { return v.id == videoId; });
}

QString PlaylistsModel::playlistName(const QString& id) const {
    const auto* p = find(id);
    return p ? p->name : QString();
}

QStringList PlaylistsModel::playlistsContaining(const QString& videoId) const {
    QStringList ids;
    if (videoId.isEmpty()) return ids;
    for (const auto& p : playlists_) {
        if (std::any_of(p.videos.begin(), p.videos.end(), [&](const Video& v) { return v.id == videoId; })) {
            ids.push_back(p.id);
        }
    }
    return ids;
}

QVariantList PlaylistsModel::summariesForVideo(const QString& videoId) const {
    QVariantList out;
    out.reserve(static_cast<int>(playlists_.size()));
    for (const auto& p : playlists_) {
        QVariantMap m;
        m.insert("id", p.id);
        m.insert("name", p.name);
        m.insert("videoCount", static_cast<int>(p.videos.size()));
        m.insert("contains", videoId.isEmpty()
                                ? false
                                : std::any_of(p.videos.begin(), p.videos.end(),
                                              [&](const Video& v) { return v.id == videoId; }));
        out.push_back(m);
    }
    return out;
}

bool PlaylistsModel::isValidName(const QString& name) const {
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return false;
    if (trimmed.length() > 100) return false;
    return true;
}

bool PlaylistsModel::isNameTaken(const QString& name, const QString& exceptId) const {
    const QString trimmed = name.trimmed();
    for (const auto& p : playlists_) {
        if (p.id == exceptId) continue;
        if (p.name.compare(trimmed, Qt::CaseInsensitive) == 0) return true;
    }
    return false;
}

void PlaylistsModel::openPlaylist(const QString& id) {
    if (currentId_ == id) return;
    const auto* p = find(id);
    if (!p) return;
    currentId_ = id;
    emit currentChanged();
}

void PlaylistsModel::closeCurrent() {
    if (currentId_.isEmpty()) return;
    currentId_.clear();
    emit currentChanged();
}

// ── Server reconcile ────────────────────────────────────────────────────────

void PlaylistsModel::refresh() {
    if (!api_) return;
    if (refreshInFlight_) {
        refreshQueued_ = true;
        return;
    }
    refreshInFlight_ = true;
    setLoading(true);

    api_->listPlaylists(
        [this](const QJsonArray& summaries) {
            reconcileSummariesFromServer(summaries);
            refreshInFlight_ = false;
            setLoading(false);
            if (refreshQueued_) {
                refreshQueued_ = false;
                refresh();
            }
        },
        [this](int code, const QString& error) {
            qWarning() << "[Playlists] refresh failed:" << code << error;
            refreshInFlight_ = false;
            setLoading(false);
            if (refreshQueued_) {
                refreshQueued_ = false;
                refresh();
            }
            // Only surface a toast for "real" failures; an offline 0 is
            // expected during boot before the network is ready, and would
            // produce a spurious banner.
            if (code != 0) emit notify(error.isEmpty() ? tr("Could not refresh playlists") : error);
        }
    );
}

void PlaylistsModel::reconcileSummariesFromServer(const QJsonArray& summaries) {
    // Build the new authoritative list. We preserve any local `videos` we
    // already have for matching ids — items aren't on the list endpoint
    // (see playlists.md §3.1) so dropping them on every refresh would
    // empty all loaded detail views. The detail page's `openPlaylist`
    // would then need to lazy-fetch, which is fine, but breaking the
    // already-rendered grid covers is worse.
    std::vector<Playlist> next;
    next.reserve(summaries.size());
    std::unordered_set<QString> seenIds;
    seenIds.reserve(summaries.size());

    for (const auto& v : summaries) {
        if (!v.isObject()) continue;
        const auto obj = v.toObject();
        Playlist p = summaryFromJson(obj);
        if (p.id.isEmpty()) continue;

        // Carry over previously-loaded items so the detail page doesn't
        // flicker. Server-reported `video_count` is authoritative for the
        // grid badge; the items themselves remain whatever we last knew.
        if (const auto* prev = find(p.id)) {
            p.videos = prev->videos;
        }

        seenIds.insert(p.id);
        next.push_back(std::move(p));
    }

    // Anything in the local mirror that the server didn't return has been
    // deleted (perhaps from another device). Drop it.
    Q_UNUSED(seenIds);  // Implicit: we just don't carry over un-listed entries.

    playlists_ = std::move(next);
    sortByName();

    beginResetModel();
    endResetModel();
    emit countChanged();

    if (!currentId_.isEmpty() && find(currentId_) == nullptr) {
        currentId_.clear();
        emit currentChanged();
    }
}

void PlaylistsModel::reconcileSingleSummary(const QJsonObject& summary) {
    auto incoming = summaryFromJson(summary);
    if (incoming.id.isEmpty()) return;

    if (auto* p = find(incoming.id)) {
        // Preserve items list (server summary doesn't include them).
        incoming.videos = std::move(p->videos);
        *p = std::move(incoming);
        sortByName();
        beginResetModel();
        endResetModel();
        if (currentId_ == p->id) emit currentChanged();
    } else {
        playlists_.push_back(std::move(incoming));
        sortByName();
        beginResetModel();
        endResetModel();
        emit countChanged();
    }
}

void PlaylistsModel::reconcileItemsFor(const QString& playlistId, const QJsonArray& items) {
    auto* p = find(playlistId);
    if (!p) return;
    p->videos.clear();
    p->videos.reserve(items.size());
    for (const auto& v : items) {
        if (!v.isObject()) continue;
        p->videos.push_back(itemFromJson(v.toObject()));
    }
    emitRowChanged(indexOf(playlistId));
    if (playlistId == currentId_) emit currentChanged();
}

// ── Static converters ────────────────────────────────────────────────────────

PlaylistsModel::Playlist PlaylistsModel::summaryFromJson(const QJsonObject& obj) {
    Playlist p;
    p.id = obj.value("id").toString();
    p.name = obj.value("name").toString();
    p.createdAt = obj.value("created_at").toString();
    p.updatedAt = obj.value("updated_at").toString();
    return p;
}

PlaylistsModel::Video PlaylistsModel::itemFromJson(const QJsonObject& obj) {
    Video v;
    v.id = obj.value("video_id").toString();
    v.title = obj.value("title").toString();
    v.url = obj.value("url").toString();
    v.size = obj.value("size").toVariant().toLongLong();
    v.mime = obj.value("mime").toString();
    v.author = obj.value("author").toString();
    v.thumbnail = obj.value("thumbnail").toString();
    v.storyboard = obj.value("storyboard").toString();
    v.description = obj.value("description").toString();
    v.addedAt = obj.value("added_at").toString();
    // The server doesn't echo back the original video's createdAt on the
    // item row (we don't ask it to — see playlists.md §2.3 fields). Leave
    // empty; the watch page falls back to `addedAt` when needed.
    return v;
}

QJsonObject PlaylistsModel::videoSnapshotForServer(const Video& v) {
    // Wire shape per playlists.md §3.7 — snake_case, all optional except
    // video_id. Server may overwrite anything we send.
    return QJsonObject{
        {"video_id", v.id},
        {"title", v.title},
        {"url", v.url},
        {"thumbnail", v.thumbnail},
        {"storyboard", v.storyboard},
        {"mime", v.mime},
        {"size", static_cast<qint64>(v.size)},
        {"author", v.author},
        {"description", v.description},
    };
}

PlaylistsModel::Video PlaylistsModel::videoFromVariant(const QVariantMap& m) {
    Video v;
    v.id = m.value("id").toString();
    v.title = m.value("title").toString();
    v.url = m.value("url").toString();
    v.size = m.value("size").toLongLong();
    v.mime = m.value("mime").toString();
    v.author = m.value("author").toString();
    v.createdAt = m.value("createdAt").toString();
    v.thumbnail = m.value("thumbnail").toString();
    v.storyboard = m.value("storyboard").toString();
    v.description = m.value("description").toString();
    v.addedAt = m.value("addedAt").toString();
    return v;
}

QVariantMap PlaylistsModel::videoToVariant(const Video& v) {
    QVariantMap m;
    m.insert("id", v.id);
    m.insert("title", v.title);
    m.insert("url", v.url);
    m.insert("size", v.size);
    m.insert("mime", v.mime);
    m.insert("author", v.author);
    m.insert("createdAt", v.createdAt);
    m.insert("thumbnail", v.thumbnail);
    m.insert("storyboard", v.storyboard);
    m.insert("description", v.description);
    m.insert("addedAt", v.addedAt);
    return m;
}

// ── Internals ───────────────────────────────────────────────────────────────

void PlaylistsModel::sortByName() {
    std::sort(playlists_.begin(), playlists_.end(), [](const Playlist& a, const Playlist& b) {
        const int cmp = a.name.localeAwareCompare(b.name);
        if (cmp != 0) return cmp < 0;
        return a.id < b.id;
    });
}

int PlaylistsModel::indexOf(const QString& id) const {
    for (int i = 0; i < static_cast<int>(playlists_.size()); ++i) {
        if (playlists_[i].id == id) return i;
    }
    return -1;
}

PlaylistsModel::Playlist* PlaylistsModel::find(const QString& id) {
    const int i = indexOf(id);
    return i >= 0 ? &playlists_[i] : nullptr;
}

const PlaylistsModel::Playlist* PlaylistsModel::find(const QString& id) const {
    const int i = indexOf(id);
    return i >= 0 ? &playlists_[i] : nullptr;
}

void PlaylistsModel::touch(Playlist& p) {
    p.updatedAt = isoNowUtc();
}

void PlaylistsModel::emitRowChanged(int row) {
    if (row < 0) return;
    const QModelIndex mi = index(row);
    emit dataChanged(mi, mi, {VideoCountRole, ThumbnailsRole, FirstThumbRole, UpdatedAtRole});
}

QStringList PlaylistsModel::previewThumbnails(const Playlist& p) const {
    QStringList out;
    for (const auto& v : p.videos) {
        if (!v.thumbnail.isEmpty()) {
            out.push_back(v.thumbnail);
            if (out.size() >= kPreviewThumbs) break;
        }
    }
    return out;
}

QString PlaylistsModel::firstThumbnail(const Playlist& p) const {
    for (const auto& v : p.videos) {
        if (!v.thumbnail.isEmpty()) return v.thumbnail;
    }
    return {};
}

QString PlaylistsModel::makeId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString PlaylistsModel::isoNowUtc() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

void PlaylistsModel::setLoading(bool v) {
    if (loading_ == v) return;
    loading_ = v;
    emit loadingChanged();
}
