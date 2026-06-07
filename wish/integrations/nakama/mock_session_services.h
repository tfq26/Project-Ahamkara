#pragma once

#include "wish/core/session_services.h"

namespace wish::integrations::nakama {

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
