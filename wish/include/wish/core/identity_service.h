#pragma once

#include "wish/types.h"

#include <chrono>
#include <string>
#include <vector>

namespace wish::core {

/// Player profile identity resolved from authentication.
struct PlayerIdentity {
    std::string player_id;       // Stable unique player identifier
    std::string display_name;    // Human-readable name (nickname)
    std::string avatar_url;      // Optional avatar URL
    std::string realm;           // Origin realm/region (e.g. "us-east", "eu-west")
    std::chrono::system_clock::time_point created_at {};  // Account creation time
};

/// Identity lookup request.
struct IdentityRequest {
    std::string token;           // Auth token identifying the player
    std::string remote_endpoint; // Network endpoint (ip:port)
};

/// Identity lookup result.
struct IdentityResult {
    bool found {false};
    PlayerIdentity identity {};
    std::string error_message;
};

/**
 * @brief Resolves player identities from auth tokens or session data.
 *
 * Concrete implementations may delegate to a backend (Nakama, custom HTTP)
 * or return in-memory mock data for testing.
 */
class IdentityService {
public:
    virtual ~IdentityService() = default;

    /// Resolve a player identity from an auth token.
    [[nodiscard]] virtual IdentityResult resolve(const IdentityRequest& request) const = 0;

    /// Lookup a player identity by stable player_id (e.g. for roster display).
    [[nodiscard]] virtual IdentityResult lookup_by_id(const std::string& player_id) const = 0;

    /// Batch lookup – resolves multiple player_ids in one call.
    [[nodiscard]] virtual std::vector<IdentityResult> lookup_batch(
        const std::vector<std::string>& player_ids) const = 0;
};

} // namespace wish::core
