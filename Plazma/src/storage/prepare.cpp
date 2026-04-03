#include "prepare.h"

#include <QFileInfo>
#include <QMimeDatabase>
#include <QStringList>

namespace storages::prepare {

PreparedFile::PreparedFile(const QString& path) : path(path), size(0) {}
PreparedFile::PreparedFile(PreparedFile&& other) = default;
PreparedFile& PreparedFile::operator=(PreparedFile&& other) = default;
PreparedFile::~PreparedFile() = default;

PreparedList::PreparedList(Error error, const QString filepath) : error(error) {}

void PrepareDetailsInParallel(const PreparedList& result, int width) {
    // ...
}

[[nodiscard]] static PreparedFile::Type DetectFileType(const QFileInfo& info) {
    static const QMimeDatabase db;
    const auto mime = db.mimeTypeForFile(info);
    const auto name = mime.name();

    if (name.startsWith("video/")) {
        return PreparedFile::Type::Video;
    }
    return PreparedFile::Type::File;
}

PreparedList ValidateMediaList(const QStringList& files) {
    auto result = PreparedList();

    result.files.reserve(files.size());

    for (const auto& file : files) {
        const auto fileinfo = QFileInfo(file);

        if (fileinfo.isDir()) {
            return {PreparedList::Error::Directory, file};
        } else if (fileinfo.size() <= 0) {
            return {PreparedList::Error::EmptyFile, file};
        } else if (fileinfo.size() > kFileSizeLimit) {
            return {PreparedList::Error::TooLargeFile, QString()};
        } else {
            auto& prepared = result.files.emplace_back(file);
            prepared.size = fileinfo.size();
            prepared.type = DetectFileType(fileinfo);
            prepared.displayName = fileinfo.fileName();
        }
    }

    PrepareDetailsInParallel(result, 0);

    return result;
}

}  // namespace storages::prepare
