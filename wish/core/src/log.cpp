#include "wish/log.h"
#include "wish/types.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <unordered_map>

namespace wish {
namespace {

// ── Level helpers ──────────────────────────────────────────────────────────

const char* level_name(LogLevel level) {
    switch (level) {
    case LogLevel::Trace:   return "Trace";
    case LogLevel::Debug:   return "Debug";
    case LogLevel::Info:    return "Info";
    case LogLevel::Warning: return "Warning";
    case LogLevel::Error:   return "Error";
    }
    return "Unknown";
}

// ── Runtime level gating ───────────────────────────────────────────────────

std::mutex g_level_mutex;
LogLevel g_min_level = LogLevel::Info;
std::unordered_map<std::string, LogLevel> g_category_levels;

bool is_enabled(std::string_view category, LogLevel level) {
    const std::lock_guard<std::mutex> lock(g_level_mutex);
    auto it = g_category_levels.find(std::string(category));
    const LogLevel effective = (it != g_category_levels.end()) ? it->second : g_min_level;
    return static_cast<int>(level) <= static_cast<int>(effective);
}

// ── Time formatting ────────────────────────────────────────────────────────

void format_time(char* buf, std::size_t buf_size) {
    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf {};
#if defined(_WIN32)
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif
    std::strftime(buf, buf_size, "%H:%M:%S", &tm_buf);
}

// ── Output helpers ─────────────────────────────────────────────────────────

/// Un-categorized output: [HH:MM:SS][Level] message
void write_uncategorized(LogLevel level, std::string_view message) {
    char time_buf[16] {};
    format_time(time_buf, sizeof(time_buf));

    // Error → stderr, everything else → stdout
    std::FILE* out = (level == LogLevel::Error) ? stderr : stdout;
    std::fprintf(out, "[%s][%s] %.*s\n",
                 time_buf, level_name(level),
                 static_cast<int>(message.size()), message.data());
}

/// Categorized output: [HH:MM:SS][Level][Category] message
void write_categorized(LogLevel level, std::string_view category,
                       std::string_view message) {
    char time_buf[16] {};
    format_time(time_buf, sizeof(time_buf));

    std::FILE* out = (level == LogLevel::Error) ? stderr : stdout;
    std::fprintf(out, "[%s][%s][%.*s] %.*s\n",
                 time_buf, level_name(level),
                 static_cast<int>(category.size()), category.data(),
                 static_cast<int>(message.size()), message.data());
}

LogLevel parse_level(std::string_view s) {
    if (s == "error"   || s == "Error")   return LogLevel::Error;
    if (s == "warning" || s == "Warning") return LogLevel::Warning;
    if (s == "info"    || s == "Info")    return LogLevel::Info;
    if (s == "debug"   || s == "Debug")   return LogLevel::Debug;
    if (s == "trace"   || s == "Trace")   return LogLevel::Trace;
    return LogLevel::Info;
}

} // anonymous namespace

// ── Uncategorized API (backward compatible) ────────────────────────────────

void log(LogLevel level, std::string_view message) {
    if (!is_enabled(WISH_LOG_CATEGORY, level)) return;
    write_uncategorized(level, message);
}

// ── Categorized API ────────────────────────────────────────────────────────

void log_info_cat(std::string_view category, std::string_view message) {
    if (!is_enabled(category, LogLevel::Info)) return;
    write_categorized(LogLevel::Info, category, message);
}

void log_warning_cat(std::string_view category, std::string_view message) {
    if (!is_enabled(category, LogLevel::Warning)) return;
    write_categorized(LogLevel::Warning, category, message);
}

void log_error_cat(std::string_view category, std::string_view message) {
    if (!is_enabled(category, LogLevel::Error)) return;
    write_categorized(LogLevel::Error, category, message);
}

void log_debug_cat(std::string_view category, std::string_view message) {
    if (!is_enabled(category, LogLevel::Debug)) return;
    write_categorized(LogLevel::Debug, category, message);
}

void log_trace_cat(std::string_view category, std::string_view message) {
    if (!is_enabled(category, LogLevel::Trace)) return;
    write_categorized(LogLevel::Trace, category, message);
}

// ── Runtime gating ─────────────────────────────────────────────────────────

bool log_enabled(std::string_view category, LogLevel level) {
    return is_enabled(category, level);
}

void set_log_level(LogLevel level) {
    const std::lock_guard<std::mutex> lock(g_level_mutex);
    g_min_level = level;
}

void set_category_log_level(std::string_view category, LogLevel level) {
    const std::lock_guard<std::mutex> lock(g_level_mutex);
    g_category_levels[std::string(category)] = level;
}

void init_log_levels_from_env() {
    const char* global_env = std::getenv("WISH_LOG_LEVEL");
    if (global_env && global_env[0] != '\0') {
        set_log_level(parse_level(global_env));
    }

    const char* cat_env = std::getenv("WISH_LOG");
    if (cat_env && cat_env[0] != '\0') {
        std::string_view spec(cat_env);
        while (!spec.empty()) {
            auto comma = spec.find(',');
            std::string_view pair = (comma == std::string_view::npos)
                                        ? spec
                                        : spec.substr(0, comma);
            auto colon = pair.find(':');
            if (colon != std::string_view::npos) {
                std::string_view cat  = trim(pair.substr(0, colon));
                std::string_view lvl  = trim(pair.substr(colon + 1));
                set_category_log_level(cat, parse_level(lvl));
            }
            if (comma == std::string_view::npos) break;
            spec = spec.substr(comma + 1);
        }
    }
}

LogLevel get_log_level() {
    const std::lock_guard<std::mutex> lock(g_level_mutex);
    return g_min_level;
}

LogLevel get_category_log_level(std::string_view category) {
    const std::lock_guard<std::mutex> lock(g_level_mutex);
    auto it = g_category_levels.find(std::string(category));
    return (it != g_category_levels.end()) ? it->second : g_min_level;
}

} // namespace wish
