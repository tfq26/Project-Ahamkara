#pragma once

#include "wish/session/session_runtime.h"
#include "wish/types.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace wish::session {

// ---------------------------------------------------------------------------
// GroupState — lifecycle phase for a session group
// ---------------------------------------------------------------------------
enum class GroupState : wish::u8 {
    /// Group created, waiting for all members to connect / be admitted.
    Lobby,
    /// Activity is running; gameplay is in progress.
    Active,
    /// Activity has ended; group is awaiting cleanup.
    Ended
};

/// Manages a group of client sessions under a single activity.
/// Each group has a unique group_id and tracks:
/// - Multiple ClientSession entries
/// - Group state (Lobby, Active, Ended)
/// - Max players, current players
/// - Activity ID binding
/// - Owner address (the player who created/leads this group)
/// - Optional fireteam_id linking back to the matchmaking fireteam
class SessionGroup {
  public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    SessionGroup(wish::u32 group_id, wish::u32 max_clients = 8);

    // Client management
    ClientSession* add_client(const wish::NetAddress& addr, time_point now);
    bool remove_client(const wish::NetAddress& addr);
    ClientSession* find_client(const wish::NetAddress& addr);

    // Group state
    [[nodiscard]] wish::u32 client_count() const;
    [[nodiscard]] wish::u32 connected_count() const;
    [[nodiscard]] bool is_full() const;

    [[nodiscard]] GroupState state() const { return state_; }
    void set_state(GroupState s) { state_ = s; }

    // Activity binding
    [[nodiscard]] wish::u64 activity_id() const { return activity_id_; }
    void set_activity_id(wish::u64 id) { activity_id_ = id; }

    // Ownership — the address of the player that owns/leads this session.
    // For fireteam-formed groups this will be the party leader of the first
    // party that was matched.
    [[nodiscard]] wish::NetAddress owner_address() const { return owner_address_; }
    void set_owner_address(const wish::NetAddress& addr) { owner_address_ = addr; }

    // Fireteam link — optional reference to the fireteam that formed this group.
    [[nodiscard]] wish::u64 fireteam_id() const { return fireteam_id_; }
    void set_fireteam_id(wish::u64 id) { fireteam_id_ = id; }

    // Timestamps
    [[nodiscard]] time_point created_at() const { return created_at_; }
    void set_created_at(time_point tp) { created_at_ = tp; }

    [[nodiscard]] time_point ended_at() const { return ended_at_; }
    void set_ended_at(time_point tp) { ended_at_ = tp; }

    // Group-level operations
    void tick(time_point now); // prune timeouts, etc.

    // Enumeration
    template <typename Fn>
    void for_each_client(Fn&& fn) {
        for (auto& client : clients_) {
            fn(client);
        }
    }

    template <typename Fn>
    void for_each_client(Fn&& fn) const {
        for (const auto& client : clients_) {
            fn(client);
        }
    }

    [[nodiscard]] wish::u32 group_id() const {
        return group_id_;
    }
    [[nodiscard]] wish::u32 max_clients() const {
        return max_clients_;
    }

  private:
    static bool same_address(const wish::NetAddress& lhs, const wish::NetAddress& rhs);

    wish::u32 group_id_;
    wish::u32 max_clients_;
    GroupState state_ {GroupState::Lobby};
    wish::u64 activity_id_ {0};
    wish::NetAddress owner_address_ {};
    wish::u64 fireteam_id_ {0};
    time_point created_at_ {};
    time_point ended_at_ {};
    std::vector<ClientSession> clients_;
    clock::duration disconnect_timeout_;
};

} // namespace wish::session
