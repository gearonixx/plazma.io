#include "fatal.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>
#include <QtCore/QTextStream>
#include <QtCore/qlogging.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string_view>

namespace plazma::fatal {

namespace {

// Hot atomics — loaded with std::memory_order_relaxed on the fatal path,
// exactly as in TdLib. They are written once from install() before anything
// else runs, so any relaxed reader that observes a non-null callback also
// observes the fully-constructed state (install() happens-before any fatal).
std::atomic<Callback> g_callback{nullptr};
std::atomic<int>      g_max_verbosity{0};
std::atomic<bool>     g_installed{false};

// Default sink: timestamp to stderr and to <AppDataLocation>/crash.log.
void default_callback(int verbosity, std::string_view message) {
    const QString ts = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    const QString line = QStringLiteral("[%1] [fatal v%2] %3\n")
                             .arg(ts)
                             .arg(verbosity)
                             .arg(QString::fromUtf8(message.data(),
                                                    static_cast<qsizetype>(message.size())));

    // stderr first — always reachable even before QStandardPaths resolves.
    std::fputs(line.toUtf8().constData(), stderr);
    std::fflush(stderr);

    // Best-effort append to a crash log under the user's app data dir.
    // QStandardPaths returns an empty string if no QCoreApplication exists yet;
    // in that case we simply skip the file write.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) return;

    QDir().mkpath(dir);
    QFile f(QDir(dir).filePath(QStringLiteral("crash.log")));
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        f.write(line.toUtf8());
        f.close();
    }
}

// Qt → plazma::fatal bridge. Only QtFatalMsg triggers process_fatal_error;
// other levels fall back to Qt's normal formatting so qDebug/qInfo/qWarning
// keep working as before.
void qt_message_handler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
    if (type == QtFatalMsg) {
        const QString formatted = qFormatLogMessage(type, ctx, msg);
        const QByteArray utf8 = formatted.toUtf8();
        process_fatal_error(std::string_view(utf8.constData(),
                                             static_cast<size_t>(utf8.size())));
        // Unreachable — process_fatal_error is [[noreturn]].
    }

    // Non-fatal: preserve the default formatting path.
    std::fputs(qFormatLogMessage(type, ctx, msg).toUtf8().constData(), stderr);
    std::fputc('\n', stderr);
}

// std::terminate → plazma::fatal bridge. Captures the current exception
// message (if any) so uncaught throws are diagnosed, not just aborted.
[[noreturn]] void terminate_handler() noexcept {
    std::string_view message = "std::terminate called";
    std::string buffer;

    if (auto eptr = std::current_exception()) {
        try {
            std::rethrow_exception(eptr);
        } catch (const std::exception& e) {
            buffer = std::string("uncaught std::exception: ") + e.what();
            message = buffer;
        } catch (...) {
            message = "uncaught non-std exception";
        }
    }

    process_fatal_error(message);
}

}  // namespace

void set_callback(Callback callback) noexcept {
    g_callback.store(callback, std::memory_order_relaxed);
}

Callback get_callback() noexcept {
    return g_callback.load(std::memory_order_relaxed);
}

void set_max_verbosity(int level) noexcept {
    g_max_verbosity.store(level, std::memory_order_relaxed);
}

int get_max_verbosity() noexcept {
    return g_max_verbosity.load(std::memory_order_relaxed);
}

void install() noexcept {
    bool expected = false;
    if (!g_installed.compare_exchange_strong(expected, true,
                                             std::memory_order_acq_rel)) {
        return;  // already installed
    }

    if (g_callback.load(std::memory_order_relaxed) == nullptr) {
        g_callback.store(&default_callback, std::memory_order_relaxed);
    }

    qInstallMessageHandler(&qt_message_handler);
    std::set_terminate(&terminate_handler);
}

// The TdLib analog. Keep the body small and branch-predictable: this is
// the last code that runs before abort, and we want it to survive in
// low-memory or stack-exhausted contexts.
void process_fatal_error(std::string_view message) noexcept {
    if (0 <= g_max_verbosity.load(std::memory_order_relaxed)) {
        auto callback = g_callback.load(std::memory_order_relaxed);
        if (callback != nullptr) {
            callback(0, message);
        }
    }

    std::abort();
}

}  // namespace plazma::fatal
