#pragma once

#include <ostream>
#include <string>
#include <string_view>

namespace wish {

/// Log severity levels. Higher numeric value = more verbose.
enum class LogLevel {
    Error = 0,
    Warning,
    Info,
    Debug,
    Trace,
};

/// Convert a LogLevel to its string name.
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

/// Stream insertion for LogLevel (useful in test assertion output).
inline std::ostream& operator<<(std::ostream& os, LogLevel level) {
    return os << to_string(level);
}

// ============================================================================
// Basic logging (uncategorized, backward-compatible convenience wrappers)
// ============================================================================

/// Unconditionally log a message at the given level (no category).
void log(LogLevel level, std::string_view message);

inline void log_info(std::string_view msg)    { log(LogLevel::Info, msg); }
inline void log_warning(std::string_view msg) { log(LogLevel::Warning, msg); }
inline void log_error(std::string_view msg)   { log(LogLevel::Error, msg); }

// ============================================================================
// Structured logging with categories
// ============================================================================
//
// Usage:
//   #define WISH_LOG_CATEGORY "Session"
//   log_info_cat(WISH_LOG_CATEGORY, "Client connected");
//
// Output format:
//   [Info][Session][HH:MM:SS] message

void log_info_cat(std::string_view category, std::string_view message);
void log_warning_cat(std::string_view category, std::string_view message);
void log_error_cat(std::string_view category, std::string_view message);

// ---------------------------------------------------------------------------
// Extended levels (Debug + Trace, gated at runtime)
// ---------------------------------------------------------------------------
//
// Disabled by default at the global level (default min level = Info).
// Enable per-category or globally via WISH_LOG_LEVEL / WISH_LOG env vars
// or programmatically via set_log_level / set_category_log_level.
//
// Usage:
//   log_debug_cat("Protocol", "Parsed packet type=" + std::to_string(type));
//   log_trace_cat("Session",  "Frame " + std::to_string(n) + " begin");

void log_debug_cat(std::string_view category, std::string_view message);
void log_trace_cat(std::string_view category, std::string_view message);

// ============================================================================
// Runtime gating
// ============================================================================

/// Returns true if `level` messages are enabled for `category`.
/// Use this to guard expensive message construction before calling
/// log_debug_cat / log_trace_cat.
[[nodiscard]] bool log_enabled(std::string_view category, LogLevel level);

/// Set the global minimum log level (applies to all categories).
/// Default: LogLevel::Info (Debug and Trace are disabled globally).
void set_log_level(LogLevel level);

/// Set the minimum log level for a specific category (overrides global).
void set_category_log_level(std::string_view category, LogLevel level);

/// Get the current global minimum log level.
[[nodiscard]] LogLevel get_log_level();

/// Get the minimum log level for a specific category, or the global default
/// if no per-category override is set.
[[nodiscard]] LogLevel get_category_log_level(std::string_view category);

/// Parse WISH_LOG_LEVEL and WISH_LOG env vars and apply them.
/// Call once at startup.
///
/// Format:
///   WISH_LOG_LEVEL=debug         (global minimum)
///   WISH_LOG=Session:trace,Protocol:debug  (per-category overrides, comma-separated)
void init_log_levels_from_env();

} // namespace wish
