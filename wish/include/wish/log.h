#pragma once

#include <string>
#include <string_view>

// ---------------------------------------------------------------------------
// WISH_LOG_CATEGORY — conventional macro for per-translation-unit category.
//
// Define before including this header to associate all uncategorized calls
// with a specific category. Falls back to "Wish" when not defined.
//
//   #define WISH_LOG_CATEGORY "Session"
//   log_info_cat(WISH_LOG_CATEGORY, "Session started");
// ---------------------------------------------------------------------------
#ifndef WISH_LOG_CATEGORY
#define WISH_LOG_CATEGORY "Wish"
#endif

namespace wish {

// Level ordering (lower value = higher priority / always-on):
//   Error (0) — always enabled
//   Warning (1) — always enabled
//   Info (2) — always enabled (default global minimum)
//   Debug (3) — disabled by default
//   Trace (4) — disabled by default
enum class LogLevel : int {
    Error = 0,
    Warning = 1,
    Info = 2,
    Debug = 3,
    Trace = 4,
};

// --- Uncategorized (backward compatible) ---

/// Returns a string representation of the log level.
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

void log(LogLevel level, std::string_view message);

inline void log_info(std::string_view msg)    { log(LogLevel::Info, msg); }
inline void log_warning(std::string_view msg) { log(LogLevel::Warning, msg); }
inline void log_error(std::string_view msg)   { log(LogLevel::Error, msg); }
inline void log_debug(std::string_view msg)   { log(LogLevel::Debug, msg); }
inline void log_trace(std::string_view msg)   { log(LogLevel::Trace, msg); }

// --- Categorized logging (preferred) ---
//
// The category is a free-form string (e.g. "Session", "NakamaBridge", "Admin").
// Define WISH_LOG_CATEGORY at the top of each translation unit for consistency:
//
//   #define WISH_LOG_CATEGORY "Session"
//   log_info_cat(WISH_LOG_CATEGORY, "Session created");

void log_info_cat(std::string_view category, std::string_view message);
void log_warning_cat(std::string_view category, std::string_view message);
void log_error_cat(std::string_view category, std::string_view message);
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

/// Parse WISH_LOG_LEVEL and WISH_LOG env vars and apply them.
/// Call once at startup. Format:
///   WISH_LOG_LEVEL=debug         (global minimum)
///   WISH_LOG=Session:trace,Admin:debug  (per-category overrides, comma-separated)
void init_log_levels_from_env();

[[nodiscard]] LogLevel get_log_level();
[[nodiscard]] LogLevel get_category_log_level(std::string_view category);

} // namespace wish
