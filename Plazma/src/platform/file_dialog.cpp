#include "file_dialog.h"

#include <QByteArray>
#include <QApplication>
#include <QObject>
#include <QPointer>
#include <QStringList>

#include <functional>
#include <memory>

#include "../core/file_utilities.h"
#include "src/storage/prepare.h"
#include "src/storage/task_queue.h"

namespace platform {

bool GetFileDialog(
    QPointer<QWidget> parent,
    QStringList& files,
    QByteArray& remoteContent,
    const QString& caption,
    const QString& filter,
    Type type
) {
    return Platform::FileDialog::Get(parent, files, remoteContent, caption, filter, type, QString());
}

FileDialog::FileDialog(Api* api, QObject* parent) : QObject(parent), api_(api) {
    Q_ASSERT(api != nullptr);
}

FileDialog::~FileDialog() = default;

void FileDialog::GetOpenPaths(
    QPointer<QWidget> parent,
    const QString& caption,
    const QString& filter,
    ResultCb callback,
    Fn<void()> failed
) {
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        [=] {
            auto files = QStringList();
            auto remoteContent = QByteArray();

            const bool success = GetFileDialog(parent, files, remoteContent, caption, filter, Type::ReadFiles);

            if (success && (!files.isEmpty() || !remoteContent.isEmpty())) {
                if (callback) {
                    auto result = DialogResult();
                    result.paths = files;
                    result.remoteContent = remoteContent;
                    callback(std::move(result));
                }
            } else if (failed) {
                failed();
            }
        },
        Qt::QueuedConnection
    );
}

[[nodiscard]] static plazma::task_queue::SendMediaType ResolveMediaType(
    const storages::prepare::PreparedFile& file,
    plazma::task_queue::SendMediaType requestedType
) {
    using FileType = storages::prepare::PreparedFile::Type;
    using SendType = plazma::task_queue::SendMediaType;

    if (file.type == FileType::Video && requestedType != SendType::File) {
        return SendType::Video;
    }
    return SendType::File;
}

void FileDialog::prepareFileTasks(
    storages::prepare::PreparedList&& bundle,
    plazma::task_queue::SendMediaType type
) {
    auto tasks = std::vector<std::unique_ptr<plazma::task_queue::Task>>();
    tasks.reserve(bundle.files.size());

    for (auto& file : bundle.files) {
        const auto mediaType = ResolveMediaType(file, type);

        tasks.push_back(std::make_unique<plazma::task_queue::FileLoadTask>(
            plazma::task_queue::FileLoadTask::Args{
                .path = file.path,
                .content = file.content,
                .size = file.size,
                .type = mediaType,
                .displayName = file.displayName,
                .onFinished = [this](const plazma::task_queue::FileLoadResult& result) {
                    api_->uploadFile(
                        "/v1/videos/upload",
                        "video",
                        result.filename,
                        result.filemime,
                        result.filedata
                    );
                },
            }
        ));
    }

    api_->fileLoader()->addTasks(std::move(tasks));
}

void FileDialog::attachFiles(plazma::task_queue::SendMediaType type) {
    GetOpenPaths(
        nullptr,
        "Open File",
        FileUtils::AllFilesFilter(),
        [this, type](const DialogResult& result) {
            if (result.paths.isEmpty() && result.remoteContent.isEmpty()) {
                return;
            }

            if (!result.paths.isEmpty()) {
                auto list = storages::prepare::ValidateMediaList(result.paths);

                if (list.error != storages::prepare::PreparedList::Error::None) {
                    return;
                }

                prepareFileTasks(std::move(list), type);
            }
        },
        nullptr
    );
}

}  // namespace platform