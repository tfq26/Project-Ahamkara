#include "wish/log.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <unordered_map>

namespace wish {
namespace {

// ============================================================================
// Global state
// ============================================================================

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

/// Global minimum log level. Defaults to Info (Debug/Trace disabled globally).
std::atomic<LogLevel> g_min_level{LogLevel::Info};

/// Per-category level overrides. Protected by a mutex.
std::mutex g_cat_mutex;
std::unordered_map<std::string, LogLevel> g_cat_levels;

/// Guards stderr output from concurrent log calls.
std::mutex g_output_mutex;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

const char* level_name(LogLevel level) {
    switch (level) {
    case LogLevel::Error:   return "Error";
    case LogLevel::Warning: return "Warning";
    case LogLevel::Info:    return "Info";
    case LogLevel::Debug:   return "Debug";
    case LogLevel::Trace:   return "Trace";
    }
    return "Unknown";
}

void format_timestamp(char* buf, std::size_t buf_size) {
    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif
    std::strftime(buf, buf_size, "%H:%M:%S", &tm_buf);
}

void vlog(LogLevel level, std::string_view category, std::string_view message) {
    // Format: [Level][Category][HH:MM:SS] message
    char time_buf[16]{};
    format_timestamp(time_buf, sizeof(time_buf));

    std::lock_guard<std::mutex> lock(g_output_mutex);

    if (category.empty()) {
        std::fprintf(stderr, "[%s][%s] %.*s\n",
                     time_buf, level_name(level),
                     static_cast<int>(message.size()), message.data());
    } else {
        std::fprintf(stderr, "[%s][%s][%s] %.*s\n",
                     level_name(level), std::string(category).c_str(), time_buf,
                     static_cast<int>(message.size()), message.data());
    }
}

} // anonymous namespace

// ============================================================================
// Basic logging (uncategorized)
// ============================================================================

void log(LogLevel level, std::string_view message) {
    vlog(level, {}, message);
}

// ============================================================================
// Structured logging with categories
// ============================================================================

void log_info_cat(std::string_view category, std::string_view message) {
    if (log_enabled(category, LogLevel::Info)) {
        vlog(LogLevel::Info, category, message);
    }
}

void log_warning_cat(std::string_view category, std::string_view message) {
    if (log_enabled(category, LogLevel::Warning)) {
        vlog(LogLevel::Warning, category, message);
    }
}

void log_error_cat(std::string_view category, std::string_view message) {
    // Error is always enabled; no gating check needed.
    vlog(LogLevel::Error, category, message);
}

void log_debug_cat(std::string_view category, std::string_view message) {
    if (log_enabled(category, LogLevel::Debug)) {
        vlog(LogLevel::Debug, category, message);
    }
}

void log_trace_cat(std::string_view category, std::string_view message) {
    if (log_enabled(category, LogLevel::Trace)) {
        vlog(LogLevel::Trace, category, message);
    }
}

// ============================================================================
// Runtime gating
// ============================================================================

bool log_enabled(std::string_view category, LogLevel level) {
    // Error is always enabled.
    if (level == LogLevel::Error) {
        return true;
    }

    const LogLevel global = g_min_level.load(std::memory_order_relaxed);

    // Check per-category override first.
    {
        std::lock_guard<std::mutex> lock(g_cat_mutex);
        const auto it = g_cat_levels.find(std::string(category));
        if (it != g_cat_levels.end()) {
            return static_cast<int>(level) <= static_cast<int>(it->second);
        }
    }

    // Fall back to global level.
    return static_cast<int>(level) <= static_cast<int>(global);
}

void set_log_level(LogLevel level) {
    g_min_level.store(level, std::memory_order_relaxed);
}

void set_category_log_level(std::string_view category, LogLevel level) {
    std::lock_guard<std::mutex> lock(g_cat_mutex);
    g_cat_levels[std::string(category)] = level;
}

LogLevel get_log_level() {
    return g_min_level.load(std::memory_order_relaxed);
}

LogLevel get_category_log_level(std::string_view category) {
    std::lock_guard<std::mutex> lock(g_cat_mutex);
    const auto it = g_cat_levels.find(std::string(category));
    if (it != g_cat_levels.end()) {
        return it->second;
    }
    return g_min_level.load(std::memory_order_relaxed);
}

void init_log_levels_from_env() {
    // WISH_LOG_LEVEL sets the global minimum.
    if (const char* raw = std::getenv("WISH_LOG_LEVEL")) {
        std::string value(raw);
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (value == "error")       set_log_level(LogLevel::Error);
        else if (value == "warning") set_log_level(LogLevel::Warning);
        else if (value == "info")    set_log_level(LogLevel::Info);
        else if (value == "debug")   set_log_level(LogLevel::Debug);
        else if (value == "trace")   set_log_level(LogLevel::Trace);
    }

    // WISH_LOG sets per-category overrides: "Cat1:level,Cat2:level"
    if (const char* raw = std::getenv("WISH_LOG")) {
        std::string_view env(raw);
        while (!env.empty()) {
            // Find the next comma or end.
            const auto comma = env.find(',');
            const std::string_view pair = env.substr(0, comma);
            env = (comma == std::string_view::npos) ? std::string_view{} : env.substr(comma + 1);

            const auto colon = pair.find(':');
            if (colon == std::string_view::npos || colon == 0 || colon + 1 >= pair.size()) {
                continue;
            }

            const std::string_view cat_name = pair.substr(0, colon);
            std::string level_str(pair.substr(colon + 1));
            std::transform(level_str.begin(), level_str.end(), level_str.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            LogLevel cat_level;
            if (level_str == "error")       cat_level = LogLevel::Error;
            else if (level_str == "warning") cat_level = LogLevel::Warning;
            else if (level_str == "info")    cat_level = LogLevel::Info;
            else if (level_str == "debug")   cat_level = LogLevel::Debug;
            else if (level_str == "trace")   cat_level = LogLevel::Trace;
            else continue;

            set_category_log_level(cat_name, cat_level);
        }
    }
}

} // namespace wish
