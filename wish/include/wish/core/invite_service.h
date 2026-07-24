#pragma once

#include "wish/types.h"

#include <chrono>
#include <string>
#include <vector>

namespace wish::core {

/// Direction of an invite.
enum class InviteDirection : wish::u8 {
    Outgoing,  // Sent by local player
    Incoming   // Received by local player
};

/// Current state of an invite.
enum class InviteStatus : wish::u8 {
    Pending,
    Accepted,
    Rejected,
    Cancelled,
    Expired
};

/// Represents a single invite between players.
struct Invite {
    std::string invite_id;                        // Unique invite identifier
    std::string from_player_id;                   // Sender
    std::string to_player_id;                     // Recipient
    std::string activity_type;                    // e.g. "deathmatch", "social_hub"
    std::string party_id;                         // Optional party/group to join
    InviteStatus status {InviteStatus::Pending};
    std::chrono::steady_clock::time_point created_at {};
    std::chrono::steady_clock::time_point expires_at {};
};

/// Result of an invite operation.
struct InviteResult {
    bool ok {false};
    std::string error_message;
    Invite invite {};
};

/// Parameters for sending a new invite.
struct SendInviteRequest {
    std::string from_player_id;
    std::string to_player_id;
    std::string activity_type;
    std::string party_id;
    std::chrono::steady_clock::duration ttl {std::chrono::seconds(30)}; // Time-to-live
};

/// Parameters for responding to an invite.
struct RespondInviteRequest {
    std::string invite_id;
    std::string player_id;  // The player responding
    bool accept {false};    // true = accept, false = reject
};

/**
 * @brief Manages player-to-player invites for activities and parties.
 *
 * Handles the full lifecycle: send, accept, reject, cancel, and expire.
 * Concrete implementations may delegate to a backend or use in-memory storage.
 */
class InviteService {
public:
    virtual ~InviteService() = default;

    /// Send an invite from one player to another.
    [[nodiscard]] virtual InviteResult send_invite(const SendInviteRequest& request) = 0;

    /// Accept a pending invite.
    [[nodiscard]] virtual InviteResult accept_invite(const RespondInviteRequest& request) = 0;

    /// Reject a pending invite.
    [[nodiscard]] virtual InviteResult reject_invite(const RespondInviteRequest& request) = 0;

    /// Cancel an outgoing invite (sender only).
    [[nodiscard]] virtual InviteResult cancel_invite(const std::string& invite_id,
                                                       const std::string& player_id) = 0;

    /// Get all pending invites for a player.
    [[nodiscard]] virtual std::vector<Invite> get_pending_invites(const std::string& player_id) const = 0;

    /// Get invites sent by a player.
    [[nodiscard]] virtual std::vector<Invite> get_outgoing_invites(const std::string& player_id) const = 0;

    /// Get invites received by a player.
    [[nodiscard]] virtual std::vector<Invite> get_incoming_invites(const std::string& player_id) const = 0;

    /// Check if a pending invite exists between two players for the given activity type.
    [[nodiscard]] virtual bool has_pending_invite(const std::string& from_player_id,
                                                    const std::string& to_player_id,
                                                    const std::string& activity_type) const = 0;

    /// Expire stale invites (call periodically).
    virtual void expire_stale_invites() = 0;
};

} // namespace wish::core
