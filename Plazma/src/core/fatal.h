#pragma once

#include <string_view>

// Fatal-error handling modeled on TdLib's process_fatal_error:
//
//   - A callback is stored as an atomic function pointer and loaded with
//     std::memory_order_relaxed — the callback is installed once at startup
//     and read on the (very rare) fatal path, so stronger ordering would be
//     wasted synchronization.
//   - `process_fatal_error` always aborts; it is [[noreturn]].
//   - `install()` wires Qt's qFatal path and std::terminate through the
//     same channel so uncaught exceptions and Qt fatals share one log sink.
namespace plazma::fatal {

// verbosity = 0 means "fatal". Mirrors TdLib's convention.
using Callback = void (*)(int verbosity, std::string_view message);

// Setters / getters.
void set_callback(Callback callback) noexcept;
Callback get_callback() noexcept;

void set_max_verbosity(int level) noexcept;
int  get_max_verbosity() noexcept;

// Route all fatal signals (qFatal, std::terminate, uncaught exceptions)
// through process_fatal_error. Installs a default callback that writes
// timestamped messages to stderr and to <AppDataLocation>/crash.log.
// Idempotent; call once from main().
void install() noexcept;

// The TdLib analog. Always aborts.
[[noreturn]] void process_fatal_error(std::string_view message) noexcept;

}  // namespace plazma::fatal
