#pragma once

#include <string_view>

namespace ae {

void log_info(std::string_view message);
void log_warning(std::string_view message);
void log_error(std::string_view message);

}  // namespace ae

