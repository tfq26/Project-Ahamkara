#pragma once

#include "wish/core/session_services.h"

#include <memory>
#include <string>
#include <string_view>

namespace wish::integrations::nakama {

struct BridgeSettings {
    bool enabled {false};
    bool use_tls {false};
    std::string host {"127.0.0.1"};
    std::string account_path {"/v2/account"};
    unsigned short port {7350};
    int timeout_ms {1500};
};

bool is_enabled(const BridgeSettings& settings) noexcept;

[[nodiscard]] BridgeSettings load_bridge_settings(int argc, char** argv);
[[nodiscard]] std::string describe_bridge(const BridgeSettings& settings);

class NakamaHttpAuthValidator final : public wish::core::AuthValidator {
public:
    explicit NakamaHttpAuthValidator(BridgeSettings settings);

    [[nodiscard]] wish::core::AuthResult validate(const wish::core::AuthRequest& request) const override;

private:
    BridgeSettings settings_;
};

[[nodiscard]] std::unique_ptr<wish::core::AuthValidator> make_auth_validator(const BridgeSettings& settings);

}  // namespace wish::integrations::nakama
