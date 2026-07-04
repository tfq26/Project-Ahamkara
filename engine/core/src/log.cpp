#include "ae/core/log.h"
#include "ae/core/cli_utils.h"
#include "ae/core/time.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ae {
namespace {

std::mutex g_log_mutex;
std::unique_ptr<std::ofstream> g_log_file;

// --- Runtime level gating ---

std::mutex g_level_mutex;
LogLevel g_min_level = LogLevel::Info;
std::unordered_map<std::string, LogLevel> g_category_levels;

void write_to_file(std::string_view formatted) {
    if (g_log_file && g_log_file->is_open()) {
        *g_log_file << formatted << std::flush;
    }
}

void log_message(std::ostream& stream, std::string_view level_str, std::string_view message) {
    const std::lock_guard<std::mutex> lock(g_log_mutex);
    const double t = now_seconds();
    const auto line = std::string("[") + std::string(level_str) + "][" +
                      std::to_string(t) + "] " + std::string(message) + '\n';
    stream << line;
    if (g_log_file && g_log_file->is_open()) {
        *g_log_file << line << std::flush;
    }
}

void log_message_cat(std::ostream& stream, std::string_view level_str,
                     std::string_view category, std::string_view message) {
    const std::lock_guard<std::mutex> lock(g_log_mutex);
    const double t = now_seconds();
    const auto line = std::string("[") + std::string(level_str) + "][" +
                      std::string(category) + "][" + std::to_string(t) + "] " +
                      std::string(message) + '\n';
    stream << line;
    if (g_log_file && g_log_file->is_open()) {
        *g_log_file << line << std::flush;
    }
}

LogLevel parse_level(std::string_view s) {
    if (s == "error" || s == "Error")   return LogLevel::Error;
    if (s == "warning" || s == "Warning") return LogLevel::Warning;
    if (s == "info" || s == "Info")     return LogLevel::Info;
    if (s == "debug" || s == "Debug")   return LogLevel::Debug;
    if (s == "trace" || s == "Trace")   return LogLevel::Trace;
    return LogLevel::Info;
}

}  // namespace

// --- Level gating API ---

bool log_enabled(std::string_view category, LogLevel level) {
    const std::lock_guard<std::mutex> lock(g_level_mutex);
    auto it = g_category_levels.find(std::string(category));
    const LogLevel effective = (it != g_category_levels.end()) ? it->second : g_min_level;
    return level <= effective;
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
    const char* global_env = std::getenv("AE_LOG_LEVEL");
    if (global_env && global_env[0] != '\0') {
        set_log_level(parse_level(global_env));
    }

    const char* cat_env = std::getenv("AE_LOG");
    if (cat_env && cat_env[0] != '\0') {
        std::string_view spec(cat_env);
        while (!spec.empty()) {
            auto comma = spec.find(',');
            std::string_view pair = (comma == std::string_view::npos) ? spec : spec.substr(0, comma);
            auto colon = pair.find(':');
            if (colon != std::string_view::npos) {
                std::string_view cat  = ae::trim(pair.substr(0, colon));
                std::string_view lvl  = ae::trim(pair.substr(colon + 1));
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

// --- Basic logging (uncategorized) ---

void log_info(std::string_view message) {
    log_message(std::cout, "Info", message);
}

void log_warning(std::string_view message) {
    log_message(std::cout, "Warning", message);
}

void log_error(std::string_view message) {
    log_message(std::cerr, "Error", message);
}

// --- Categorized logging ---

void log_info_cat(std::string_view category, std::string_view message) {
    if (!log_enabled(category, LogLevel::Info)) return;
    log_message_cat(std::cout, "Info", category, message);
}

void log_warning_cat(std::string_view category, std::string_view message) {
    if (!log_enabled(category, LogLevel::Warning)) return;
    log_message_cat(std::cout, "Warning", category, message);
}

void log_error_cat(std::string_view category, std::string_view message) {
    if (!log_enabled(category, LogLevel::Error)) return;
    log_message_cat(std::cerr, "Error", category, message);
}

void log_debug_cat(std::string_view category, std::string_view message) {
    if (!log_enabled(category, LogLevel::Debug)) return;
    log_message_cat(std::cout, "Debug", category, message);
}

void log_trace_cat(std::string_view category, std::string_view message) {
    if (!log_enabled(category, LogLevel::Trace)) return;
    log_message_cat(std::cout, "Trace", category, message);
}

// --- File logging ---

void init_file_logging(const std::filesystem::path& log_dir) {
    std::error_code ec;
    std::filesystem::create_directories(log_dir, ec);
    if (ec) {
        log_warning_cat("Core", "Failed to create log directory " + log_dir.string() + ": " + ec.message());
        return;
    }
    const auto log_path = log_dir / "ahamkara.log";
    g_log_file = std::make_unique<std::ofstream>(log_path, std::ios::app);
    if (g_log_file && g_log_file->is_open()) {
        log_info_cat("Core", "Logging to " + log_path.string() + " started.");
    } else {
        log_warning_cat("Core", "Could not open log file: " + log_path.string());
    }
}

void shutdown_file_logging() {
    if (g_log_file && g_log_file->is_open()) {
        log_info_cat("Core", "Logging stopped.");
    } else {
        log_debug_cat("Core", "shutdown_file_logging: no log file was open (init may not have been called).");
    }
    g_log_file.reset();
}

}  // namespace ae
