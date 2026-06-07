#pragma once

#include <string_view>

namespace wish::core {

struct EngineIdentity {
    std::string_view name;
    std::string_view version;
};

const EngineIdentity& identity() noexcept;

}  // namespace wish::core
