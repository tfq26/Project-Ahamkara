#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ae {

enum class LogLevel {
    Error = 0,
    Warning,
    Info,
    Debug,
    Trace,
};

constexpr std::string_view to_string(LogLevel level) {
    using namespace std::string_view_literals;
    switch (level) {
        case LogLevel::Error:   return "Error"sv;
        case LogLevel::Warning: return "Warning"sv;
        case LogLevel::Info:    return "Info"sv;
        case LogLevel::Debug:   return "Debug"sv;
        case LogLevel::Trace:   return "Trace"sv;
    }
    return "Unknown"sv;
}

// --- Basic logging (uncategorized, kept for backward compatibility) ---

void log_info(std::string_view message);
void log_warning(std::string_view message);
void log_error(std::string_view message);

// --- Structured logging with categories ---
//
// Usage:
//   #define AE_LOG_CATEGORY "Network"
//   log_info_cat(AE_LOG_CATEGORY, "Packet received");
// The category is prepended to the log line: [Info][Network][0.123] Packet received

void log_info_cat(std::string_view category, std::string_view message);
void log_warning_cat(std::string_view category, std::string_view message);
void log_error_cat(std::string_view category, std::string_view message);

// --- Extended levels (Debug + Trace, gated at runtime) ---
//
// Usage:
//   log_debug_cat(AE_LOG_CATEGORY, "Resource resolved: " + path);
//   log_trace_cat(AE_LOG_CATEGORY, "Frame " + std::to_string(n));
// Disabled by default; enable per-category via AE_LOG env var.

void log_debug_cat(std::string_view category, std::string_view message);
void log_trace_cat(std::string_view category, std::string_view message);

// --- Runtime gating ---

/// Returns true if `level` messages are enabled for `category`.
/// Use this to guard expensive message construction before calling
/// log_debug_cat / log_trace_cat.
[[nodiscard]] bool log_enabled(std::string_view category, LogLevel level);

/// Set the global minimum log level (applies to all categories).
/// Default: LogLevel::Info (Debug and Trace are disabled globally).
void set_log_level(LogLevel level);

/// Set the minimum log level for a specific category (overrides global).
void set_category_log_level(std::string_view category, LogLevel level);

/// Parse AE_LOG_LEVEL and AE_LOG env vars and apply them.
/// Call once at startup. Format:
///   AE_LOG_LEVEL=debug         (global minimum)
///   AE_LOG=Render:trace,Core:debug  (per-category overrides, comma-separated)
void init_log_levels_from_env();

[[nodiscard]] LogLevel get_log_level();
[[nodiscard]] LogLevel get_category_log_level(std::string_view category);

// --- File‑backed logging ---

void init_file_logging(const std::filesystem::path& log_dir = "logs");
void shutdown_file_logging();

}  // namespace ae
