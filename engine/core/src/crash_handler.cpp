#include "ae/core/crash_handler.h"
#include "ae/core/log.h"
#include "ae/core/time.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>

#include <csignal>
#include <unistd.h>

// execinfo.h for backtrace — available on macOS/BSD, glibc on Linux
#include <execinfo.h>

#define AE_LOG_CATEGORY "CrashHandler"

namespace ae {

namespace {

/// Saved previous signal handlers so we can restore them.
struct SavedHandlers {
    struct sigaction segv;
    struct sigaction abrt;
    struct sigaction fpe;
    struct sigaction ill;
    struct sigaction bus;
};
SavedHandlers g_saved{};

/// Global pointer to the CrashHandler instance, set at install time.
/// Used by the C signal handler trampoline (must be async-signal-safe).
CrashHandler* g_handler_instance = nullptr;

/// Buffer for crash-time stack trace capture (pre-allocated, async-signal-safe).
constexpr int kMaxFrames = 64;

void crash_signal_handler(int signum, siginfo_t* info, void* /*ucontext*/) {
    // Reinstall default handler to avoid infinite recursion
    signal(signum, SIG_DFL);

    CrashHandler* handler = g_handler_instance;
    if (!handler || !handler->enabled()) {
        // Re-raise with default handler
        raise(signum);
        return;
    }

    // Capture stack trace using async-signal-safe backtrace()
    void* buffer[kMaxFrames];
    int frame_count = backtrace(buffer, kMaxFrames);

    CrashContext ctx;
    ctx.signal_num = signum;
    ctx.signal_name = signal_name(signum);
    ctx.fault_addr = (info->si_addr != nullptr) ? info->si_addr : nullptr;
    ctx.timestamp_sec = now_seconds();

    // Resolve symbols (NOT async-signal-safe, but useful for post-mortem)
    char** symbols = backtrace_symbols(buffer, frame_count);
    if (symbols) {
        for (int i = 1; i < frame_count; ++i) {  // skip this frame
            StackFrame frame;
            frame.address = reinterpret_cast<std::uintptr_t>(buffer[i]);
            frame.symbol = symbols[i] ? symbols[i] : "??";
            ctx.frames.push_back(std::move(frame));
        }
        free(symbols); // NOLINT: backtrace_symbols malloc'd
    } else {
        // Fallback: store raw addresses
        for (int i = 1; i < frame_count; ++i) {
            StackFrame frame;
            frame.address = reinterpret_cast<std::uintptr_t>(buffer[i]);
            frame.symbol = "<symbols unavailable>";
            ctx.frames.push_back(std::move(frame));
        }
    }

    // Write crash dump
    auto dump_path = write_crash_dump(ctx, handler->crash_dir());

    // Invoke callbacks (use accessor to avoid friend dependency)
    const auto& callbacks = handler->crash_callbacks();
    for (auto& cb : callbacks) {
        if (cb) cb(ctx);
    }

    // Log to stderr
    std::string msg = "CRASH: " + ctx.signal_name + " (signal " + std::to_string(signum) + ")";
    if (ctx.fault_addr) {
        msg += " at address " + std::to_string(reinterpret_cast<std::uintptr_t>(ctx.fault_addr));
    }
    msg += "\nCrash dump written to: " + dump_path.string();
    log_error_cat(AE_LOG_CATEGORY, msg);

    // Re-raise with default handler for OS crash report
    raise(signum);
}

}  // anonymous namespace

// ===================================================================
// CrashHandler
// ===================================================================

CrashHandler::CrashHandler() = default;

CrashHandler::~CrashHandler() {
    uninstall();
}

CrashHandler& CrashHandler::instance() {
    static CrashHandler handler;
    return handler;
}

void CrashHandler::install() {
    if (installed_) return;
    if (!enabled_) return;

    g_handler_instance = this;

    struct sigaction sa{};
    sa.sa_sigaction = crash_signal_handler;
    sa.sa_flags = SA_SIGINFO;  // use siginfo_t to get fault address

    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGSEGV);
    sigaddset(&sa.sa_mask, SIGABRT);
    sigaddset(&sa.sa_mask, SIGFPE);
    sigaddset(&sa.sa_mask, SIGILL);
    sigaddset(&sa.sa_mask, SIGBUS);

    sigaction(SIGSEGV, &sa, &g_saved.segv);
    sigaction(SIGABRT, &sa, &g_saved.abrt);
    sigaction(SIGFPE,  &sa, &g_saved.fpe);
    sigaction(SIGILL,  &sa, &g_saved.ill);
    sigaction(SIGBUS,  &sa, &g_saved.bus);

    installed_ = true;
    log_info_cat(AE_LOG_CATEGORY, "Crash handler installed for SIGSEGV/SIGABRT/SIGFPE/SIGILL/SIGBUS");
}

void CrashHandler::uninstall() {
    if (!installed_) return;

    sigaction(SIGSEGV, &g_saved.segv, nullptr);
    sigaction(SIGABRT, &g_saved.abrt, nullptr);
    sigaction(SIGFPE,  &g_saved.fpe,  nullptr);
    sigaction(SIGILL,  &g_saved.ill,  nullptr);
    sigaction(SIGBUS,  &g_saved.bus,  nullptr);

    g_handler_instance = nullptr;
    installed_ = false;
    log_info_cat(AE_LOG_CATEGORY, "Crash handler uninstalled");
}

void CrashHandler::on_crash(CrashCallback cb) {
    callbacks_.push_back(std::move(cb));
}

// ===================================================================
// Free functions
// ===================================================================

std::vector<StackFrame> capture_stack_trace(int skip_frames) {
    void* buffer[kMaxFrames];
    int frame_count = backtrace(buffer, kMaxFrames);

    std::vector<StackFrame> frames;
    char** symbols = backtrace_symbols(buffer, frame_count);
    if (symbols) {
        for (int i = skip_frames; i < frame_count; ++i) {
            StackFrame frame;
            frame.address = reinterpret_cast<std::uintptr_t>(buffer[i]);
            frame.symbol = symbols[i] ? symbols[i] : "??";
            frames.push_back(std::move(frame));
        }
        free(symbols); // NOLINT
    }
    return frames;
}

const char* signal_name(int signum) {
    switch (signum) {
        case SIGSEGV: return "SIGSEGV";
        case SIGABRT: return "SIGABRT";
        case SIGFPE:  return "SIGFPE";
        case SIGILL:  return "SIGILL";
        case SIGBUS:  return "SIGBUS";
        case SIGINT:  return "SIGINT";
        case SIGTERM: return "SIGTERM";
        default:      return "UNKNOWN";
    }
}

std::filesystem::path write_crash_dump(const CrashContext& ctx, const std::filesystem::path& crash_dir) {
    std::error_code ec;
    std::filesystem::create_directories(crash_dir, ec);
    if (ec) {
        log_error_cat(AE_LOG_CATEGORY, "Failed to create crash directory: " + crash_dir.string());
        return {};
    }

    // Build filename: crash_<unix_timestamp>_<signalname>.dmp
    auto ts = static_cast<std::int64_t>(ctx.timestamp_sec);
    std::string filename = "crash_" + std::to_string(ts) + "_" + ctx.signal_name + ".dmp";
    auto filepath = crash_dir / filename;

    std::ofstream file(filepath);
    if (!file.is_open()) {
        log_error_cat(AE_LOG_CATEGORY, "Failed to open crash dump: " + filepath.string());
        return {};
    }

    file << "=== Crash Dump ===" << "\n";
    file << "Signal: " << ctx.signal_name << " (" << ctx.signal_num << ")" << "\n";
    file << "Timestamp: " << ctx.timestamp_sec << "\n";
    if (ctx.fault_addr) {
        file << "Fault Address: " << ctx.fault_addr << "\n";
    }
    file << "\n=== Stack Trace ===" << "\n";
    for (std::size_t i = 0; i < ctx.frames.size(); ++i) {
        file << "  #" << i << " 0x"
             << std::hex << ctx.frames[i].address << std::dec
             << " " << ctx.frames[i].symbol << "\n";
    }
    file << "\n=== Log Tail ===" << "\n";
    file << "<log tail not captured at crash time>" << "\n";

    return filepath;
}

CrashContext read_crash_dump(const std::filesystem::path& path) {
    CrashContext ctx;
    std::ifstream file(path);
    if (!file.is_open()) return ctx;

    std::string line;
    while (std::getline(file, line)) {
        if (line.find("Signal: ") == 0) {
            auto rest = line.substr(8);
            auto paren = rest.find(" (");
            if (paren != std::string::npos) {
                ctx.signal_name = rest.substr(0, paren);
                auto num_str = rest.substr(paren + 2, rest.find(')') - paren - 2);
                ctx.signal_num = std::atoi(num_str.c_str());
            }
        }
        if (line.find("Timestamp: ") == 0) {
            ctx.timestamp_sec = std::atof(line.substr(11).c_str());
        }
        if (line.find("Fault Address: ") == 0) {
            // stored as hex pointer; parse it
            auto addr_str = line.substr(16);
            std::uintptr_t addr_val = 0;
            std::stringstream ss;
            ss << std::hex << addr_str;
            ss >> addr_val;
            ctx.fault_addr = reinterpret_cast<void*>(addr_val);
        }
        if (line.find("  #") == 0) {
            StackFrame frame;
            // Format: "  #<index> 0x<hex_addr> <symbol>"
            // Skip "  #<index> " to get to "0x<addr> <symbol>"
            auto addr_start = line.find("0x");
            if (addr_start != std::string::npos) {
                auto addr_hex = line.substr(addr_start + 2);  // skip "0x"
                auto space = addr_hex.find(' ');
                if (space != std::string::npos) {
                    auto addr_str = addr_hex.substr(0, space);
                    std::stringstream ss;
                    ss << std::hex << addr_str;
                    ss >> frame.address;
                    frame.symbol = addr_hex.substr(space + 1);
                }
            }
            ctx.frames.push_back(std::move(frame));
        }
    }
    return ctx;
}

std::vector<std::filesystem::path> list_crash_dumps(const std::filesystem::path& crash_dir) {
    std::vector<std::filesystem::path> dumps;
    std::error_code ec;

    if (!std::filesystem::exists(crash_dir, ec)) return dumps;

    for (const auto& entry : std::filesystem::directory_iterator(crash_dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".dmp") {
            dumps.push_back(entry.path());
        }
    }

    // Sort by modification time, most recent first
    std::sort(dumps.begin(), dumps.end(),
              [](const std::filesystem::path& a, const std::filesystem::path& b) {
                  std::error_code ec_a, ec_b;
                  auto ta = std::filesystem::last_write_time(a, ec_a);
                  auto tb = std::filesystem::last_write_time(b, ec_b);
                  if (ec_a || ec_b) return false;
                  return ta > tb;
              });

    return dumps;
}

}  // namespace ae
