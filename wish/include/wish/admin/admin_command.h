#pragma once

#include <string_view>

namespace wish::admin {

struct AdminCommand {
    std::string_view name;
    std::string_view description;
};

}  // namespace wish::admin
