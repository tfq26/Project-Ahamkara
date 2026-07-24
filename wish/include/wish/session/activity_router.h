#pragma once

#include "wish/types.h"
#include "wish/session/activity_session.h"
#include "wish/session/session_group.h"

#include <chrono>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

namespace wish::session {

/// Manages multiple ActivitySession instances and routes clients to the
/// correct session based on activity category.
///
/// The router owns the ActivitySession objects and provides factory methods
/// for creating, finding, and removing sessions. It also supports routing
/// a client to an appropriate session given a desired activity category.
///
/// Game-neutral: works with any ActivityCategory and delegates all
/// game-specific behavior to the bound IActivityBase.
class ActivityRouter {
public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    ActivityRouter() = default;

    // ── Session factory ─────────────────────────────────────────────────

    /// Create a new ActivitySession with the given configuration.
    /// Returns a pointer to the created session, or nullptr if allocation
    /// fails or the session already exists.
    [[nodiscard]] ActivitySession* create_session(wish::u32 id,
                                                   const ActivitySessionConfig& config);

    /// Create a new ActivitySession with default configuration for the
    /// given category and max players.
    [[nodiscard]] ActivitySession* create_session(wish::u32 id,
                                                   wish::core::ActivityCategory category,
                                                   wish::u32 max_players = 8);

    // ── Lookup ──────────────────────────────────────────────────────────

    /// Find a session by its unique id.
    [[nodiscard]] ActivitySession* find_session(wish::u32 id);
    [[nodiscard]] const ActivitySession* find_session(wish::u32 id) const;

    /// Find all sessions matching a given category.
    [[nodiscard]] std::vector<ActivitySession*> find_sessions_by_category(wish::core::ActivityCategory category);

    /// Find all sessions in a given lifecycle state.
    [[nodiscard]] std::vector<ActivitySession*> find_sessions_by_state(ActivitySessionState state);

    /// Find a lobby-stage session that is not yet full for the given
    /// category. Returns nullptr if no suitable session exists.
    [[nodiscard]] ActivitySession* find_available_lobby(wish::core::ActivityCategory category) const;

    // ── Routing ─────────────────────────────────────────────────────────

    /// Route a client to a suitable session for the given category.
    /// Tries to find an available lobby first; if none exists, creates a
    /// new session and returns it.
    /// @param addr        Client address.
    /// @param category    Desired activity category.
    /// @param now         Current time.
    /// @param out_session Populated with the session the client was added to.
    /// @return true if the client was successfully routed.
    bool route_client(const wish::NetAddress& addr,
                      wish::core::ActivityCategory category,
                      time_point now,
                      ActivitySession** out_session = nullptr);

    // ── Removal ─────────────────────────────────────────────────────────

    /// Remove and destroy a session.
    /// Returns true if the session was found and removed.
    bool remove_session(wish::u32 id);

    // ── Global operations ───────────────────────────────────────────────

    /// Tick all non-terminal sessions.
    void tick_all(float dt, time_point now);

    /// Return the total number of managed sessions.
    [[nodiscard]] std::size_t session_count() const { return sessions_.size(); }

    /// Count sessions by state.
    [[nodiscard]] wish::u32 count_by_state(ActivitySessionState state) const;

    /// Count sessions by category.
    [[nodiscard]] wish::u32 count_by_category(wish::core::ActivityCategory category) const;

    /// Access all sessions (read-only).
    [[nodiscard]] const auto& sessions() const { return sessions_; }

    /// Clear all sessions.
    void clear() { sessions_.clear(); }

private:
    std::unordered_map<wish::u32, std::unique_ptr<ActivitySession>> sessions_;
};

}  // namespace wish::session
