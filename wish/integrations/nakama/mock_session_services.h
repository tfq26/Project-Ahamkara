#pragma once

#include "wish/core/session_services.h"

#include <string>

namespace wish::integrations::nakama {

/// Auth validator that denies all requests (fail-closed).
///
/// Used in production when no backend validator is configured and the
/// process is not running in development mode.  Clients receive a stable
/// WS-AUT-1001 error and must be configured with a real auth backend.
class DenyAllAuthValidator final : public wish::core::AuthValidator {
public:
    [[nodiscard]] wish::core::AuthResult validate(const wish::core::AuthRequest&) const override {
        wish::core::AuthResult result {};
        result.accepted = false;
        result.player_id = {};
        result.session_id = {};
        result.error_message = "Authentication denied: no auth backend configured and WISH_DEV_MODE is not set. "
                               "Set WISH_DEV_MODE=1 for development or configure a backend validator.";
        return result;
    }
};

/// Auth validator that accepts all requests (development mode only).
class NoopAuthValidator final : public wish::core::AuthValidator {
public:
    [[nodiscard]] wish::core::AuthResult validate(const wish::core::AuthRequest& request) const override {
        wish::core::AuthResult result {};
        result.accepted = true;
        result.player_id = request.remote_endpoint.empty() ? "wish-player" : "wish-player@" + request.remote_endpoint;
        result.session_id = request.token.empty() ? "wish-session" : request.token;
        return result;
    }
};

class NoopSessionAdmissionService final : public wish::core::SessionAdmissionService {
public:
    [[nodiscard]] wish::core::SessionAdmissionResult admit(
        const wish::core::SessionAdmissionRequest& request) const override {
        wish::core::SessionAdmissionResult result {};
        result.admitted = true;
        result.match_id = request.session_id.empty() ? "wish-match" : request.session_id;
        return result;
    }
};

class NoopMatchResultReporter final : public wish::core::MatchResultReporter {
public:
    void report_match_result(const wish::core::MatchResult&) override {}
};

}  // namespace wish::integrations::nakama
