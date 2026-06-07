#pragma once

namespace wish::integrations::nakama {

struct BridgeSettings {
    bool enabled = false;
};

bool is_enabled(const BridgeSettings& settings) noexcept;

}  // namespace wish::integrations::nakama
