#include "download_paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>
#include <array>

namespace plazma::download_paths {
namespace {

using namespace Qt::Literals::StringLiterals;

constexpr int kMaxFilenameChars = 120;
constexpr int kMaxCollisionAttempts = 999;

constexpr auto kKnownVideoExtensions = std::to_array<QLatin1StringView>({
    "mp4"_L1, "mkv"_L1, "webm"_L1, "mov"_L1, "avi"_L1,
    "flv"_L1, "wmv"_L1, "m4v"_L1, "ts"_L1,
});


[[nodiscard]] bool isKnownVideoExtension(QStringView ext) {
    return std::find(kKnownVideoExtensions.begin(), kKnownVideoExtensions.end(), ext)
        != kKnownVideoExtensions.end();
}

}  // namespace

QString defaultRoot() {
    auto base = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (base.isEmpty()) base = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (base.isEmpty()) base = QDir::homePath();
    return QDir(base).filePath(QStringLiteral("Plazma"));
}

QString sanitizeFilename(const QString& title) {
    static const QRegularExpression illegal(QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1F]"));
    auto clean = title.trimmed();
    clean.replace(illegal, QStringLiteral("_"));

    static const QRegularExpression ws(QStringLiteral("\\s+"));
    clean.replace(ws, QStringLiteral(" "));

    // Windows reserved device names (CON, PRN, AUX, NUL, COM1-9, LPT1-9) are
    // illegal even when followed by an extension. Sidestep with a prefix
    // underscore so the user still recognizes the title.
    static const QRegularExpression reserved(
        QStringLiteral("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$"),
        QRegularExpression::CaseInsensitiveOption
    );
    if (reserved.match(clean).hasMatch()) clean = QStringLiteral("_") + clean;

    if (clean.size() > kMaxFilenameChars) clean = clean.left(kMaxFilenameChars).trimmed();

    // Windows silently strips trailing dots / spaces from filenames, which
    // would make our "already exists?" check lie to us — strip them here so
    // the path we hand back is the path the OS will actually create.
    while (!clean.isEmpty() && (clean.endsWith(QLatin1Char(' ')) || clean.endsWith(QLatin1Char('.')))) {
        clean.chop(1);
    }
    return clean;
}

QString extensionFor(const QString& sourceUrl, const QString& mime) {
    const QUrl url(sourceUrl);
    const auto urlExt = QFileInfo(url.path()).suffix().toLower();
    if (isKnownVideoExtension(urlExt)) return urlExt;

    if (!mime.isEmpty()) {
        QMimeDatabase db;
        const auto type = db.mimeTypeForName(mime);
        if (type.isValid()) {
            const auto pref = type.preferredSuffix().toLower();
            if (!pref.isEmpty()) return pref;
        }
    }
    return QStringLiteral("mp4");
}

QString computeDestPath(
    const QString& root,
    const QString& title,
    const QString& sourceUrl,
    const QString& mime
) {
    QDir().mkpath(root);

    auto base = sanitizeFilename(title);
    if (base.isEmpty()) {
        // Last-resort: pull the final path segment off the URL (without
        // query string). Usually produces something like "abc123" for
        // presigned URLs.
        const QUrl url(sourceUrl);
        base = sanitizeFilename(QFileInfo(url.path()).completeBaseName());
    }
    if (base.isEmpty()) base = QStringLiteral("video");

    const auto ext = extensionFor(sourceUrl, mime);

    QString candidate = QStringLiteral("%1/%2.%3").arg(root, base, ext);

    int n = 1;
    while (QFile::exists(candidate) || QFile::exists(candidate + QStringLiteral(".part"))) {
        candidate = QStringLiteral("%1/%2 (%3).%4").arg(root, base).arg(n).arg(ext);
        ++n;
        if (n > kMaxCollisionAttempts) break;
    }
    return candidate;
}

QString rederivePath(
    const QString& previousDestPath,
    const QString& title,
    const QString& sourceUrl,
    const QString& mime
) {
    const QString root = previousDestPath.isEmpty()
        ? defaultRoot()
        : QFileInfo(previousDestPath).absolutePath();
    return computeDestPath(root, title, sourceUrl, mime);
}

}  // namespace plazma::download_paths
