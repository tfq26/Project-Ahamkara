#pragma once

#include "wish/types.h"
#include "wish/session/session_group.h"
#include "wish/core/activity.h"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string_view>

namespace wish::session {

/// Lifecycle states for an ActivitySession.
/// Transitions:
///   Idle -> Lobby -> Active -> Completed
///   Idle -> Lobby -> Cancelled
///   Idle -> Cancelled
enum class ActivitySessionState : wish::u8 {
    Idle = 0,       ///< Initial state, no lobby started
    Lobby = 1,      ///< Accepting players, pre-launch
    Active = 2,     ///< Activity is running
    Completed = 3,  ///< Activity ended normally
    Cancelled = 4   ///< Activity was cancelled before/during play
};

/// Returns a human-readable name for the state.
[[nodiscard]] constexpr std::string_view activity_session_state_name(ActivitySessionState state) {
    switch (state) {
        case ActivitySessionState::Idle:      return "Idle";
        case ActivitySessionState::Lobby:     return "Lobby";
        case ActivitySessionState::Active:    return "Active";
        case ActivitySessionState::Completed: return "Completed";
        case ActivitySessionState::Cancelled: return "Cancelled";
    }
    return "Unknown";
}

/// Configuration for creating an ActivitySession.
struct ActivitySessionConfig {
    wish::core::ActivityCategory category {wish::core::ActivityCategory::PvP};
    wish::u32 max_players {8};
    std::chrono::seconds lobby_timeout {std::chrono::seconds(120)};
    bool allow_spectators {false};
};

/// Ties a session group to an activity lifecycle.
///
/// ActivitySession owns a SessionGroup and manages lobby/launch/completion
/// state transitions. The activity instance pointer is non-owning — the
/// caller (e.g. ActivityManager) retains ownership.
///
/// This type is game-neutral: it references IActivityBase* but does not
/// depend on game-specific commands or snapshots.
class ActivitySession {
public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    /// Construct a new ActivitySession.
    /// @param id       Unique session identifier.
    /// @param config   Configuration (category, max_players, lobby timeout, etc.).
    explicit ActivitySession(wish::u32 id, ActivitySessionConfig config = {});

    // ── Identity ────────────────────────────────────────────────────────

    [[nodiscard]] wish::u32 id() const { return id_; }
    [[nodiscard]] wish::core::ActivityCategory category() const { return config_.category; }
    [[nodiscard]] ActivitySessionState state() const { return state_; }
    [[nodiscard]] const ActivitySessionConfig& config() const { return config_; }

    // ── Client management (delegates to SessionGroup) ───────────────────

    /// Add a client to the session group. Only accepted in Lobby state.
    /// Returns nullptr if the group is full or the session is not in Lobby.
    [[nodiscard]] ClientSession* add_client(const wish::NetAddress& addr, time_point now);

    /// Remove a client from the session group.
    bool remove_client(const wish::NetAddress& addr);

    /// Find a client by address.
    [[nodiscard]] ClientSession* find_client(const wish::NetAddress& addr);

    [[nodiscard]] wish::u32 client_count() const { return group_.client_count(); }
    [[nodiscard]] wish::u32 connected_count() const { return group_.connected_count(); }
    [[nodiscard]] bool is_full() const { return group_.is_full(); }

    /// Enumerate clients in the group.
    template <typename Fn>
    void for_each_client(Fn&& fn) { group_.for_each_client(std::forward<Fn>(fn)); }

    template <typename Fn>
    void for_each_client(Fn&& fn) const { group_.for_each_client(std::forward<Fn>(fn)); }

    // ── Lifecycle ───────────────────────────────────────────────────────

    /// Move the session to Lobby state.
    /// Returns false if the transition is invalid (not in Idle).
    bool start_lobby(time_point now);

    /// Move the session to Active state and bind an activity instance.
    /// The activity pointer is non-owning; the caller must keep it alive.
    /// Returns false if the transition is invalid.
    bool launch(time_point now, core::IActivityBase* activity);

    /// Mark the session as Completed.
    /// Returns false if the transition is invalid.
    bool complete();

    /// Cancel the session from any non-terminal state.
    /// Returns false if already in a terminal state (Completed/Cancelled).
    bool cancel();

    /// Advance the session. Handles lobby timeouts and delegates tick
    /// to the bound activity if in Active state.
    void tick(float dt, time_point now);

    // ── Activity binding ────────────────────────────────────────────────

    /// Get the bound activity instance (nullptr if not launched).
    [[nodiscard]] core::IActivityBase* activity() const { return activity_; }

    /// Direct access to the underlying SessionGroup (for advanced use).
    [[nodiscard]] SessionGroup& group() { return group_; }
    [[nodiscard]] const SessionGroup& group() const { return group_; }

    // ── Timing ──────────────────────────────────────────────────────────

    /// Time elapsed since the last state transition.
    [[nodiscard]] clock::duration time_in_state(time_point now) const;

    /// Time point when the current state was entered.
    [[nodiscard]] time_point state_entered_at() const { return state_entered_; }

private:
    void set_state(ActivitySessionState new_state, time_point now);

    wish::u32 id_;
    ActivitySessionConfig config_;
    ActivitySessionState state_ {ActivitySessionState::Idle};
    time_point state_entered_{};
    SessionGroup group_;
    core::IActivityBase* activity_ {nullptr};
};

}  // namespace wish::session
