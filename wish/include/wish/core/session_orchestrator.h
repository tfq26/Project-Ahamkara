#pragma once

#include "wish/types.h"
#include "wish/core/matchmaking_service.h"
#include "wish/core/activity_manager.h"
#include "wish/session/party.h"
#include "wish/session/session_group.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace wish::core {

// ---------------------------------------------------------------------------
// JoinRequest / JoinResult — typed join/leave contract
// ---------------------------------------------------------------------------

/// A player's request to join a session group.
struct JoinRequest {
    wish::NetAddress player_address {};
    wish::u64 party_id {0};
    wish::u64 group_id {0};
    std::string player_identity {};
};

struct JoinResult {
    bool accepted {false};
    wish::u32 group_id {0};
    std::string error_message {};
};

/// A player's intent to leave a session group.
struct LeaveRequest {
    wish::NetAddress player_address {};
    wish::u64 party_id {0};
    wish::u64 group_id {0};
};

struct LeaveResult {
    bool removed {false};
    bool group_ended {false};       ///< true if the group is now empty/ended
    std::string error_message {};
};

// ---------------------------------------------------------------------------
// OwnershipPolicy — who is allowed to perform lifecycle operations
// ---------------------------------------------------------------------------
enum class OwnershipPolicy : wish::u8 {
    /// Only the registered owner (e.g. party leader) can disband / end.
    Strict,
    /// Any connected client in the group can trigger ending.
    Lax,
    /// No ownership checks — service-level decisions only.
    None
};

// ---------------------------------------------------------------------------
// SessionOrchestrator — bridges matchmaking → session → activity
// ---------------------------------------------------------------------------
//
// Responsibilities:
//   - Registers as the match-found callback on MatchmakingService
//   - Creates SessionGroups from matched fireteams
//   - Provides join/leave methods that handle Party ↔ SessionGroup mapping
//   - Tracks owner → group mapping for ownership validation
//   - Connects sessions to ActivityManager
//
class SessionOrchestrator {
  public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    /// Configuration for the orchestrator.
    struct Config {
        OwnershipPolicy ownership_policy {OwnershipPolicy::Strict};
        bool auto_activate_sessions {true};
        bool auto_end_on_empty {true};
    };

    SessionOrchestrator() = default;
    explicit SessionOrchestrator(Config cfg);

    // -- Lifecycle hooks ----------------------------------------------------

    /// Callback fired when a session group transitions to Active state.
    using SessionActivatedCallback = std::function<void(wish::u32 group_id, wish::u64 activity_id)>;

    /// Callback fired when a session group transitions to Ended state.
    using SessionEndedCallback = std::function<void(wish::u32 group_id, wish::u64 activity_id)>;

    void set_on_session_activated(SessionActivatedCallback cb) {
        on_session_activated_ = std::move(cb);
    }
    void set_on_session_ended(SessionEndedCallback cb) {
        on_session_ended_ = std::move(cb);
    }

    // -- Matchmaking integration --------------------------------------------

    /// Attach a MatchmakingService. This orchestrator will register as the
    /// match-found callback so it can create session groups automatically.
    void attach_matchmaking(MatchmakingService& service);

    /// Attach an ActivityManager for session-to-activity binding.
    void attach_activity_manager(ActivityManager& mgr) {
        activity_manager_ = &mgr;
    }

    /// Process a match-found event manually (also called from the callback).
    /// Creates a SessionGroup from the fireteam and records ownership.
    /// Returns the group_id of the created session group, or 0 on failure.
    wish::u32 on_match_found(const Fireteam& fireteam, time_point now);

    // -- Party / Session binding --------------------------------------------

    /// Record that a party has been assigned to a session group.
    void bind_party_to_group(wish::u64 party_id, wish::u32 group_id);

    /// Remove a party's binding to a session group.
    void unbind_party_from_group(wish::u64 party_id);

    /// Find which group a party is assigned to. Returns 0 if not found.
    [[nodiscard]] wish::u32 party_group(wish::u64 party_id) const;

    // -- Join / Leave flow --------------------------------------------------

    /// Process a join request for a session group.
    /// Validates ownership if the policy is Strict and the group has an owner.
    JoinResult process_join(const JoinRequest& request, time_point now);

    /// Process a leave request for a session group.
    /// If auto_end_on_empty is set and the group becomes empty, transitions
    /// the group to Ended state.
    LeaveResult process_leave(const LeaveRequest& request, time_point now);

    // -- Ownership ----------------------------------------------------------

    /// Check whether an address is the owner of a given group.
    [[nodiscard]] bool is_owner(const wish::NetAddress& addr, wish::u32 group_id) const;

    /// Transfer ownership of a group to a different address.
    /// Returns false if the group does not exist or the new owner is not a member.
    bool transfer_ownership(const wish::NetAddress& new_owner, wish::u32 group_id);

    // -- Session lifecycle --------------------------------------------------

    /// Activate a session group (transition Lobby → Active).
    /// Returns false if the group does not exist or is not in Lobby state.
    bool activate_session(wish::u32 group_id, time_point now);

    /// End a session group (transition → Ended).
    /// Returns false if the group does not exist.
    bool end_session(wish::u32 group_id, time_point now);

    /// Remove and destroy an ended session group.
    /// Returns false if the group is not in Ended state.
    bool destroy_group(wish::u32 group_id);

    // -- Queries ------------------------------------------------------------

    /// Find a session group by id (non-const).
    [[nodiscard]] session::SessionGroup* find_group(wish::u32 group_id);

    /// Find a session group by id (const).
    [[nodiscard]] const session::SessionGroup* find_group(wish::u32 group_id) const;

    /// Number of session groups currently tracked.
    [[nodiscard]] wish::u32 group_count() const;

    /// Number of groups in each state.
    [[nodiscard]] wish::u32 lobby_count() const;
    [[nodiscard]] wish::u32 active_count() const;
    [[nodiscard]] wish::u32 ended_count() const;

    [[nodiscard]] const Config& config() const { return config_; }

    // -- Enumeration --------------------------------------------------------

    template <typename Fn>
    void for_each_group(Fn&& fn) {
        for (auto& [id, group] : groups_) {
            fn(group);
        }
    }

    template <typename Fn>
    void for_each_group(Fn&& fn) const {
        for (const auto& [id, group] : groups_) {
            fn(group);
        }
    }

  private:
    /// Generate a unique group id.
    wish::u32 next_group_id();

    Config config_;
    MatchmakingService* matchmaking_service_ {nullptr};
    ActivityManager* activity_manager_ {nullptr};

    /// Session groups keyed by group_id.
    std::unordered_map<wish::u32, session::SessionGroup> groups_;

    /// Party → group_id mapping for join/leave tracking.
    std::unordered_map<wish::u64, wish::u32> party_to_group_;

    /// Group → owner address for ownership checks.
    std::unordered_map<wish::u32, wish::NetAddress> group_owner_;

    wish::u32 next_group_id_ {1};

    SessionActivatedCallback on_session_activated_ {};
    SessionEndedCallback on_session_ended_ {};
};

} // namespace wish::core
