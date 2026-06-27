#include "ae/core/log.h"
#include "ae/core/time.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string_view>

namespace ae {
namespace {

std::mutex g_log_mutex;
std::unique_ptr<std::ofstream> g_log_file;

void write_to_file(std::string_view formatted) {
    if (g_log_file && g_log_file->is_open()) {
        *g_log_file << formatted << std::flush;
    }
}

void log_message(std::ostream& stream, std::string_view level, std::string_view message) {
    const std::lock_guard<std::mutex> lock(g_log_mutex);
    stream << "[" << level << "]"
           << "[" << now_seconds() << "] "
           << message << '\n';
    if (g_log_file && g_log_file->is_open()) {
        *g_log_file << "[" << level << "]"
                    << "[" << now_seconds() << "] "
                    << message << '\n'
                    << std::flush;
    }
}

void log_message_cat(std::ostream& stream, std::string_view level, std::string_view category, std::string_view message) {
    const std::lock_guard<std::mutex> lock(g_log_mutex);
    stream << "[" << level << "]"
           << "[" << category << "]"
           << "[" << now_seconds() << "] "
           << message << '\n';
    if (g_log_file && g_log_file->is_open()) {
        *g_log_file << "[" << level << "]"
                    << "[" << category << "]"
                    << "[" << now_seconds() << "] "
                    << message << '\n'
                    << std::flush;
    }
}

}  // namespace

void log_info(std::string_view message) {
    log_message(std::cout, "Info", message);
}

void log_warning(std::string_view message) {
    log_message(std::cout, "Warning", message);
}

void log_error(std::string_view message) {
    log_message(std::cerr, "Error", message);
}

void log_info_cat(std::string_view category, std::string_view message) {
    log_message_cat(std::cout, "Info", category, message);
}

void log_warning_cat(std::string_view category, std::string_view message) {
    log_message_cat(std::cout, "Warning", category, message);
}

void log_error_cat(std::string_view category, std::string_view message) {
    log_message_cat(std::cerr, "Error", category, message);
}

void init_file_logging(const std::filesystem::path& log_dir) {
    std::filesystem::create_directories(log_dir);
    const auto log_path = log_dir / "ahamkara.log";
    g_log_file = std::make_unique<std::ofstream>(log_path, std::ios::app);
    if (g_log_file && g_log_file->is_open()) {
        log_info("Logging to " + log_path.string() + " started.");
    }
}

void shutdown_file_logging() {
    if (g_log_file && g_log_file->is_open()) {
        log_info("Logging stopped.");
    }
    g_log_file.reset();
}

}  // namespace ae
