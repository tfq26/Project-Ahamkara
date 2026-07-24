#pragma once

#include "wish/types.h"

#include <string>
#include <vector>

namespace wish::core {

/// A player's presence status.
enum class PresenceStatus : wish::u8 {
    Offline,
    Online,
    InGame,
    Away
};

/// A single entry in a player's roster (friend/contact list).
struct RosterEntry {
    std::string player_id;
    std::string display_name;
    PresenceStatus status {PresenceStatus::Offline};
    bool is_friend {false};
    bool is_blocked {false};
};

/// Result of a roster operation.
struct RosterResult {
    bool ok {false};
    std::string error_message;
    std::vector<RosterEntry> entries {};
};

/**
 * @brief Manages player rosters (friends, contacts, blocked players).
 *
 * Provides operations for viewing, adding, and removing roster entries.
 * Concrete implementations may delegate to a backend or use in-memory storage.
 */
class RosterService {
public:
    virtual ~RosterService() = default;

    /// Fetch the full roster for a given player.
    [[nodiscard]] virtual RosterResult get_roster(const std::string& player_id) const = 0;

    /// Fetch only online roster entries.
    [[nodiscard]] virtual RosterResult get_online_roster(const std::string& player_id) const = 0;

    /// Add a player to the roster (send friend request or direct add).
    [[nodiscard]] virtual RosterResult add_entry(const std::string& owner_id,
                                                   const std::string& target_player_id) = 0;

    /// Remove a player from the roster.
    [[nodiscard]] virtual RosterResult remove_entry(const std::string& owner_id,
                                                      const std::string& target_player_id) = 0;

    /// Block a player (removes from roster and prevents future invites).
    [[nodiscard]] virtual RosterResult block_player(const std::string& owner_id,
                                                      const std::string& target_player_id) = 0;

    /// Unblock a previously blocked player.
    [[nodiscard]] virtual RosterResult unblock_player(const std::string& owner_id,
                                                        const std::string& target_player_id) = 0;

    /// Check if owner has blocked target (for invite validation).
    [[nodiscard]] virtual bool is_blocked(const std::string& owner_id,
                                           const std::string& target_player_id) const = 0;
};

} // namespace wish::core
