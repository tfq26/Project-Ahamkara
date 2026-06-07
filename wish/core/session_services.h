#pragma once

#include <string>

namespace wish::core {

struct AuthRequest {
    std::string token;
    std::string remote_endpoint;
};

struct AuthResult {
    bool accepted {false};
    std::string player_id;
    std::string session_id;
    std::string error_message;
};

class AuthValidator {
public:
    virtual ~AuthValidator() = default;

    [[nodiscard]] virtual AuthResult validate(const AuthRequest& request) const = 0;
};

struct SessionAdmissionRequest {
    std::string player_id;
    std::string session_id;
    std::string remote_endpoint;
};

struct SessionAdmissionResult {
    bool admitted {false};
    std::string match_id;
    std::string error_message;
};

class SessionAdmissionService {
public:
    virtual ~SessionAdmissionService() = default;

    [[nodiscard]] virtual SessionAdmissionResult admit(const SessionAdmissionRequest& request) const = 0;
};

struct MatchResult {
    std::string match_id;
    std::string player_id;
    bool completed {false};
    std::string summary;
};

class MatchResultReporter {
public:
    virtual ~MatchResultReporter() = default;

    virtual void report_match_result(const MatchResult& result) = 0;
};

}  // namespace wish::core
