#include "wish/log.h"

#include <chrono>
#include <cstdio>
#include <string>

namespace wish {
namespace {

const char* level_name(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:
        return "Debug";
    case LogLevel::Info:
        return "Info";
    case LogLevel::Warning:
        return "Warning";
    case LogLevel::Error:
        return "Error";
    }
    return "Unknown";
}

} // anonymous namespace

void log(LogLevel level, std::string_view message) {
    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);

    // Format time as HH:MM:SS
    std::tm tm_buf {};
#if defined(_WIN32)
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif
    char time_buf[16] {};
    std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &tm_buf);

    std::fprintf(stderr, "[%s][%s] %.*s\n",
                 time_buf, level_name(level),
                 static_cast<int>(message.size()), message.data());
}

} // namespace wish
