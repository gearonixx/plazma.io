#pragma once

#include <QString>
#include <deque>
#include <string>
#include <vector>

#include "../basic_types.h"

namespace storages::prepare {

constexpr auto kFileSizeLimit = 2'000 * int64(1024 * 1024);

struct PreparedFile final {
    PreparedFile(const QString& path);
    PreparedFile(PreparedFile&& other);
    PreparedFile& operator=(PreparedFile&& other);
    ~PreparedFile();

    enum class Type { None, Video, File };

    QString path;
    QByteArray content;
    int64 size;
    QString displayName;

    Type type = Type::File;
};

// why this thing is a struct?
struct PreparedList final {
    enum class Error { None, Directory, EmptyFile, TooLargeFile };

    explicit PreparedList() = default;
    PreparedList(Error error, const QString filepath);

public:
    Error error = Error::None;

    std::vector<PreparedFile> files;
    std::deque<PreparedFile> filesToProcess;
};

void PrepareDetailsInParallel(const PreparedList& result, int width);
PreparedList ValidateMediaList(const QStringList& files);

};  // namespace storages::prepare
