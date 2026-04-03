#include "task_queue.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMutexLocker>
#include <QThread>

namespace plazma::task_queue {

TaskQueue::TaskQueue() {}

TaskQueue::~TaskQueue() {
    if (_thread) {
        _thread->requestInterruption();
        _thread->quit();
        _thread->wait();
        delete _thread;
    }
}

TaskId TaskQueue::addTask(std::unique_ptr<Task>&& task) {
    const auto result = task->id();
    {
        QMutexLocker lock(&_tasksToProcessMutex);
        _tasksToProcess.push_back(std::move(task));
    }

    wakeThread();

    return result;
}

void TaskQueue::addTasks(std::vector<std::unique_ptr<Task>>&& tasks) {
    {
        QMutexLocker lock(&_tasksToProcessMutex);
        for (auto& task : tasks) {
            _tasksToProcess.push_back(std::move(task));
        }
    }

    wakeThread();
}

void TaskQueue::wakeThread() {
    if (!_thread) {
        _thread = new QThread();

        _worker = new TaskQueueWorker(this);
        _worker->moveToThread(_thread);

        connect(this, &TaskQueue::taskAdded, _worker, &TaskQueueWorker::onTaskAdded);
        connect(_worker, &TaskQueueWorker::taskProcessed, this, &TaskQueue::onTaskProcessed);

        _thread->start();
    }

    emit taskAdded();
}

void TaskQueue::cancelTask(TaskId id) {
    const auto removeFrom = [&](Queue& queue) {
        const auto it = std::find_if(queue.begin(), queue.end(), [id](const std::unique_ptr<Task>& t) {
            return t->id() == id;
        });
        if (it != queue.end()) {
            queue.erase(it);
        }
    };

    {
        QMutexLocker lock(&_tasksToProcessMutex);
        removeFrom(_tasksToProcess);
        if (_taskInProcessId == id) {
            _taskInProcessId = kEmptyTaskId;
        }
    }

    QMutexLocker lock(&_tasksToFinishMutex);
    removeFrom(_tasksToFinish);
}

void TaskQueue::onTaskProcessed() {
    do {
        auto task = std::unique_ptr<Task>();
        {
            QMutexLocker lock(&_tasksToFinishMutex);
            if (_tasksToFinish.empty()) break;
            task = std::move(_tasksToFinish.front());
            _tasksToFinish.pop_front();
        }
        task->finish();
    } while (true);
}

void TaskQueue::stop() {
    if (_thread) {
        _thread->requestInterruption();
        _thread->quit();
        _thread->wait();
    }
}

// TaskQueueWorker

void TaskQueueWorker::onTaskAdded() {
    if (_inTaskAdded) return;
    _inTaskAdded = true;

    bool someTasksLeft = false;

    do {
        auto task = std::unique_ptr<Task>();
        {
            QMutexLocker lock(&_queue->_tasksToProcessMutex);
            if (!_queue->_tasksToProcess.empty()) {
                task = std::move(_queue->_tasksToProcess.front());
                _queue->_tasksToProcess.pop_front();
                _queue->_taskInProcessId = task->id();
            }
        }

        if (task) {
            task->process();

            bool emitTaskProcessed = false;
            {
                QMutexLocker lockToProcess(&_queue->_tasksToProcessMutex);
                if (_queue->_taskInProcessId == task->id()) {
                    _queue->_taskInProcessId = kEmptyTaskId;
                    someTasksLeft = !_queue->_tasksToProcess.empty();

                    QMutexLocker lockToFinish(&_queue->_tasksToFinishMutex);
                    emitTaskProcessed = _queue->_tasksToFinish.empty();
                    _queue->_tasksToFinish.push_back(std::move(task));
                }
            }

            if (emitTaskProcessed) {
                emit taskProcessed();
            }
        }

        QCoreApplication::processEvents();
    } while (someTasksLeft && !QThread::currentThread()->isInterruptionRequested());

    _inTaskAdded = false;
}

FileLoadTask::FileLoadTask(Args&& args)
    : _filepath(std::move(args.path)),
      _content(std::move(args.content)),
      _size(args.size),
      _type(args.type),
      _displayName(std::move(args.displayName)),
      _onFinished(std::move(args.onFinished)) {}

FileLoadTask::~FileLoadTask() = default;

void FileLoadTask::process() {
    _result = std::make_unique<FileLoadResult>();
    _result->type = _type;

    static const QMimeDatabase mimeDb;
    const auto info = QFileInfo(_filepath);

    if (!info.exists() || info.isDir()) {
        _result->filesize = -1;
        return;
    }

    _result->filepath = _filepath;
    _result->filename = _displayName.isEmpty() ? info.fileName() : _displayName;
    _result->filesize = info.size();
    _result->filemime = mimeDb.mimeTypeForFile(info).name();

    if (_result->filesize <= 0 || _result->filesize > storages::prepare::kFileSizeLimit) {
        return;
    }

    QFile f(_filepath);
    if (f.open(QIODevice::ReadOnly)) {
        _result->filedata = f.readAll();
    }

    qDebug() << "[FileLoadTask] processed:" << _result->filename
             << "mime:" << _result->filemime
             << "size:" << _result->filedata.size() << "bytes";
}

void FileLoadTask::finish() {
    if (_result && _result->filesize > 0 && _onFinished) {
        qDebug() << "[FileLoadTask] finish -> uploading:" << _result->filename;
        _onFinished(*_result);
    } else {
        qWarning() << "[FileLoadTask] finish -> skipped (no data or no callback)";
    }
}

}  // namespace plazma::task_queue
