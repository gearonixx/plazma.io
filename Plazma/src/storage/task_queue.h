#pragma once

#include <QMutex>
#include <QObject>
#include <QThread>
#include <deque>
#include <functional>

#include <QByteArray>
#include <QString>

#include "../basic_types.h"
#include "../storage/prepare.h"

namespace plazma::task_queue {

enum class SendMediaType {
    Video,
    File,
};

using TaskId = void*;
inline constexpr auto kEmptyTaskId = TaskId();

class Task {
public:
    virtual ~Task() = default;
    virtual void process() = 0;
    virtual void finish() = 0;

    TaskId id() const {
        Task* ptr = const_cast<Task*>(this);
        return static_cast<TaskId>(ptr);
    }
};

struct FileLoadResult {
    QString filepath;
    QString filename;
    QString filemime;
    int64 filesize = 0;

    QByteArray filedata;

    SendMediaType type = SendMediaType::File;
};

class FileLoadTask : public Task {
public:
    void process() override;
    void finish() override;

    using Callback = Fn<void(const FileLoadResult&)>;

    struct Args {
        QString path;
        QByteArray content;
        int64 size;
        SendMediaType type = SendMediaType::File;
        QString displayName;
        Callback onFinished;
    };

    explicit FileLoadTask(Args&& args);

    ~FileLoadTask();

    const FileLoadResult* result() const { return _result.get(); }

private:
    QString _filepath;
    QByteArray _content;
    int64 _size = 0;
    SendMediaType _type = SendMediaType::File;
    QString _displayName;
    Callback _onFinished;

    std::unique_ptr<FileLoadResult> _result;
};

class TaskQueue : public QObject {
    Q_OBJECT

public:
    explicit TaskQueue();
    ~TaskQueue();

    TaskId addTask(std::unique_ptr<Task>&& task);
    void addTasks(std::vector<std::unique_ptr<Task>>&& tasks);
    void cancelTask(TaskId id);

    void wakeThread();

signals:
    void taskAdded();

public slots:
    void onTaskProcessed();
    void stop();

private:
    friend class TaskQueueWorker;

    using Queue = std::deque<std::unique_ptr<Task>>;

    Queue _tasksToProcess;
    Queue _tasksToFinish;

    TaskId _taskInProcessId = kEmptyTaskId;

    QThread* _thread = nullptr;
    class TaskQueueWorker* _worker = nullptr;

    QMutex _tasksToProcessMutex;
    QMutex _tasksToFinishMutex;
};

class TaskQueueWorker : public QObject {
    Q_OBJECT

public:
    explicit TaskQueueWorker(TaskQueue* queue) : _queue(queue) {}

signals:
    void taskProcessed();

public slots:
    void onTaskAdded();

private:
    TaskQueue* _queue = nullptr;
    bool _inTaskAdded = false;
};

}  // namespace plazma::task_queue
