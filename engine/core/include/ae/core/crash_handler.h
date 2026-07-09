#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace ae {

/**
 * @brief Stack frame information captured during a crash.
 */
struct StackFrame {
    std::uintptr_t address{0};
    std::string symbol;
    std::string offset;
};

/**
 * @brief Collected crash context written to the crash dump file.
 */
struct CrashContext {
    /// Signal number (e.g. SIGSEGV = 11)
    int signal_num{0};
    /// Signal name string
    std::string signal_name;
    /// Fault address (for SIGSEGV/SIGBUS)
    void* fault_addr{nullptr};
    /// Timestamp (seconds since epoch)
    double timestamp_sec{0};
    /// Captured stack frames
    std::vector<StackFrame> frames;
};

/**
 * @brief Capture a stack trace from the current execution context.
 *
 * Uses backtrace() / backtrace_symbols() on Apple/POSIX platforms.
 * Returns resolved frame info without signal-handler safety guarantees
 * (best-effort, may allocate).
 */
[[nodiscard]] std::vector<StackFrame> capture_stack_trace(int skip_frames = 1);

/**
 * @brief Write a crash dump to the crash output directory.
 *
 * Format:
 *   crashes/crash_<timestamp>_<signal>.dmp
 *
 * Contains: signal info, fault address, timestamp, stack trace,
 * and a log tail section placeholder.
 *
 * @return The path to the written dump file, or empty on failure.
 */
[[nodiscard]] std::filesystem::path write_crash_dump(
    const CrashContext& ctx,
    const std::filesystem::path& crash_dir = "crashes");

/**
 * @brief Read a previously written crash dump file.
 * Returns empty context if parsing fails.
 */
[[nodiscard]] CrashContext read_crash_dump(const std::filesystem::path& path);

/**
 * @brief List crash dump files in the crash directory, sorted by
 *        modification time (most recent first).
 */
[[nodiscard]] std::vector<std::filesystem::path> list_crash_dumps(
    const std::filesystem::path& crash_dir = "crashes");

// -----------------------------------------------------------------------
// CrashHandler — installs POSIX signal handlers at startup
// -----------------------------------------------------------------------

using CrashCallback = std::function<void(const CrashContext& ctx)>;

/**
 * @brief Installs signal handlers and manages crash dump output.
 *
 * Usage:
 * @code
 *   CrashHandler handler;
 *   handler.install();
 *   // ... runtime code ...
 *   // If a crash occurs, handler writes a dump and calls registered callbacks
 *   handler.uninstall();  // optional, done automatically at destruction
 * @endcode
 *
 * Config-driven: respects `crash_handler.enabled` ConfigVar.
 * Crash dumps written to `crashes/` directory by default.
 */
class CrashHandler {
public:
    CrashHandler();
    ~CrashHandler();

    CrashHandler(const CrashHandler&) = delete;
    CrashHandler& operator=(const CrashHandler&) = delete;
    CrashHandler(CrashHandler&&) = delete;
    CrashHandler& operator=(CrashHandler&&) = delete;

    /// Install signal handlers (SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS).
    void install();

    /// Uninstall signal handlers (restores previous).
    void uninstall();

    /// Whether handlers are currently installed.
    [[nodiscard]] bool installed() const { return installed_; }

    /// Set crash output directory.
    void set_crash_dir(std::filesystem::path dir) { crash_dir_ = std::move(dir); }
    [[nodiscard]] const std::filesystem::path& crash_dir() const { return crash_dir_; }

    /// Register a callback invoked when a crash occurs.
    /// Callbacks are called from the signal handler; they must be
    /// async-signal-safe or use a pre-allocated buffer.
    void on_crash(CrashCallback cb);

    /// Enable/disable crash handler.
    void set_enabled(bool enabled) { enabled_ = enabled; }
    [[nodiscard]] bool enabled() const { return enabled_; }

    /// Return the single global instance (used by signal handlers).
    static CrashHandler& instance();

    /// Access registered callbacks (for signal handler trampoline).
    [[nodiscard]] const std::vector<CrashCallback>& crash_callbacks() const { return callbacks_; }

private:
    bool installed_{false};
    bool enabled_{true};
    std::filesystem::path crash_dir_{"crashes"};
    std::vector<CrashCallback> callbacks_;
};

/// Signal name table lookup.
[[nodiscard]] const char* signal_name(int signum);

}  // namespace ae
