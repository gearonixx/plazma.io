#include "settings_model.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QUrl>
#include <functional>
#include <utility>

#include "src/storage/download_paths.h"

namespace paths = plazma::download_paths;

SettingsModel::SettingsModel(std::shared_ptr<Settings> settings, QObject* parent)
    : QObject(parent), settings_(std::move(settings)) {
    Q_ASSERT(settings_);

    // Settings is the single source of truth; relay its change notification
    // outward so QML bindings on `downloadPath` / `effectiveDownloadPath`
    // re-evaluate without a manual refresh.
    connect(settings_.get(), &Settings::downloadPathChanged,
            this, &SettingsModel::downloadPathChanged);
}

QString SettingsModel::downloadPath() const {
    return settings_->getDownloadPath();
}

QString SettingsModel::effectiveDownloadPath() const {
    const auto raw = settings_->getDownloadPath();
    return raw.isEmpty() ? paths::defaultRoot() : raw;
}

QString SettingsModel::defaultDownloadPath() const {
    return paths::defaultRoot();
}

bool SettingsModel::usingDefaultDownloadPath() const {
    return settings_->getDownloadPath().isEmpty();
}

// Centralised validation + persist. Both the picker and the manual text
// commit funnel through this so the rules stay in lock-step — there's
// only one place to look when "why did my path get rejected?" comes up.
namespace {

bool validateAndPersist(
    QString path,
    Settings& settings,
    std::function<void(const QString&)> reportError
) {
    path = path.trimmed();
    if (path.isEmpty()) {
        reportError(SettingsModel::tr("Folder path can't be empty."));
        return false;
    }

    // Expand a leading "~/" so users can paste shell-style paths from a
    // terminal. We don't do mid-string expansion — those pretty much
    // never round-trip cleanly, and getting it wrong silently changes
    // where files land.
    if (path == QStringLiteral("~") || path.startsWith(QStringLiteral("~/"))) {
        path = QDir::homePath() + path.mid(1);
    }

    const QFileInfo info(path);
    if (!info.exists()) {
        reportError(SettingsModel::tr("That folder doesn't exist."));
        return false;
    }
    if (!info.isDir()) {
        reportError(SettingsModel::tr("That path isn't a folder."));
        return false;
    }
    if (!info.isWritable()) {
        reportError(SettingsModel::tr("Plazma can't write to that folder."));
        return false;
    }

    // Fold a redundant pick of the platform default back into the
    // "default" sentinel, so flipping to and from the OS default is a
    // true no-op rather than freezing today's path into QSettings.
    const auto normalized = QDir::cleanPath(path);
    if (normalized == QDir::cleanPath(paths::defaultRoot())) {
        settings.setDownloadPath(QString());
    } else {
        settings.setDownloadPath(normalized);
    }
    return true;
}

}  // namespace

QString SettingsModel::chooseDownloadFolder() {
    // Seed the dialog with the current effective folder so the picker
    // opens somewhere familiar. Make sure it exists so the dialog doesn't
    // silently fall back to the user's home directory.
    const auto seed = effectiveDownloadPath();
    QDir().mkpath(seed);

    // DontUseNativeDialog forces Qt's own (grey, cross-platform) picker
    // — same look on Linux, Windows, macOS, and same look in light or
    // dark theme. Matches OBS Studio's UX where the picker stays in
    // the app's visual language rather than yanking the user into the
    // OS file manager.
    const auto picked = QFileDialog::getExistingDirectory(
        nullptr,
        tr("Choose download folder"),
        seed,
        QFileDialog::ShowDirsOnly
            | QFileDialog::DontResolveSymlinks
            | QFileDialog::DontUseNativeDialog
    );
    if (picked.isEmpty()) return {};   // user cancelled

    const bool ok = validateAndPersist(picked, *settings_,
        [this](const QString& reason) { emit downloadPathError(reason); });
    return ok ? effectiveDownloadPath() : QString();
}

bool SettingsModel::setManualDownloadPath(QString path) {
    return validateAndPersist(std::move(path), *settings_,
        [this](const QString& reason) { emit downloadPathError(reason); });
}

void SettingsModel::resetDownloadPath() {
    settings_->setDownloadPath(QString());
}

void SettingsModel::revealDownloadFolder() {
    const auto path = effectiveDownloadPath();
    QDir().mkpath(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}
