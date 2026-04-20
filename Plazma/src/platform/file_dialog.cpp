#include "file_dialog.h"

#include <QByteArray>
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QStringList>
#include <QTemporaryFile>

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

// Run the local ffmpeg binary to pull a single frame from a video file. Returns
// JPEG bytes on success, or an empty QByteArray if ffmpeg isn't installed / the
// video is too short / anything else goes wrong. The server has its own extractor,
// so this is purely an optimistic "seed the feed with a thumbnail immediately"
// optimization — a failure here just means the feed shows a thumbnail slightly
// later (once the server's async job finishes).
[[nodiscard]] static QByteArray ExtractClientThumbnail(const QString& video_path) {
    if (video_path.isEmpty() || !QFileInfo::exists(video_path)) return {};

    QTemporaryFile out_file(QStringLiteral("plazma_thumb_XXXXXX.jpg"));
    out_file.setAutoRemove(true);
    if (!out_file.open()) return {};
    const QString out_path = out_file.fileName();
    out_file.close();

    QProcess ff;
    ff.setProcessChannelMode(QProcess::MergedChannels);
    ff.start(QStringLiteral("ffmpeg"), QStringList{
        "-y", "-hide_banner", "-loglevel", "error", "-nostdin",
        "-ss", "3",
        "-i", video_path,
        "-frames:v", "1",
        "-vf", "scale=480:-2",
        "-q:v", "3",
        "-f", "image2",
        out_path,
    });
    if (!ff.waitForStarted(2000)) return {};   // ffmpeg not installed
    if (!ff.waitForFinished(5000)) { ff.kill(); return {}; }
    if (ff.exitStatus() != QProcess::NormalExit || ff.exitCode() != 0) {
        // Retry without -ss for short videos where the seek target is past EOF.
        QProcess retry;
        retry.setProcessChannelMode(QProcess::MergedChannels);
        retry.start(QStringLiteral("ffmpeg"), QStringList{
            "-y", "-hide_banner", "-loglevel", "error", "-nostdin",
            "-i", video_path,
            "-frames:v", "1",
            "-vf", "scale=480:-2",
            "-q:v", "3",
            "-f", "image2",
            out_path,
        });
        if (!retry.waitForFinished(5000)) { retry.kill(); return {}; }
        if (retry.exitStatus() != QProcess::NormalExit || retry.exitCode() != 0) return {};
    }

    QFile f(out_path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
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

        const auto source_path = file.path;
        const bool is_video    = (mediaType == plazma::task_queue::SendMediaType::Video);

        tasks.push_back(std::make_unique<plazma::task_queue::FileLoadTask>(
            plazma::task_queue::FileLoadTask::Args{
                .path = file.path,
                .content = file.content,
                .size = file.size,
                .type = mediaType,
                .displayName = file.displayName,
                .onFinished = [this, source_path, is_video](const plazma::task_queue::FileLoadResult& result) {
                    // Capture a local frame for an instant thumbnail; empty if
                    // ffmpeg isn't available — server will fill it in later.
                    QByteArray thumb;
                    if (is_video) {
                        thumb = ExtractClientThumbnail(source_path);
                    }
                    api_->uploadFile(
                        "/v1/videos/upload",
                        "video",
                        result.filename,
                        result.filemime,
                        result.filedata,
                        thumb
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
                emit pathsPicked(result.paths);

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