#include "ae/core/log.h"

#include "ae/core/time.h"

#include <iostream>
#include <mutex>
#include <string_view>

namespace ae {
namespace {

std::mutex g_log_mutex;

void log_message(std::ostream& stream, std::string_view level, std::string_view message) {
    const std::lock_guard<std::mutex> lock(g_log_mutex);
    stream << "[" << level << "]"
           << "[" << now_seconds() << "] "
           << message << '\n';
}

void log_message_cat(std::ostream& stream, std::string_view level, std::string_view category, std::string_view message) {
    const std::lock_guard<std::mutex> lock(g_log_mutex);
    stream << "[" << level << "]"
           << "[" << category << "]"
           << "[" << now_seconds() << "] "
           << message << '\n';
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

}  // namespace ae

