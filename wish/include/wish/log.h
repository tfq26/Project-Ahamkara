#pragma once

#include <string>
#include <string_view>

namespace wish {

enum class LogLevel { Debug,
                      Info,
                      Warning,
                      Error };

void log(LogLevel level, std::string_view message);

inline void log_info(std::string_view msg) {
    log(LogLevel::Info, msg);
}
inline void log_warning(std::string_view msg) {
    log(LogLevel::Warning, msg);
}
inline void log_error(std::string_view msg) {
    log(LogLevel::Error, msg);
}

} // namespace wish
