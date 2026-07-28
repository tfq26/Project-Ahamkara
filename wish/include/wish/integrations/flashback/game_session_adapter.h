#pragma once

#include "wish/core/activity.h"
#include "wish/core/session_services.h"
#include "wish/core/error_envelope.h"
#include "wish/types.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace wish::integrations::flashback {

/// Game-neutral representation of a player's game state.
/// Wish does NOT expose game-specific types like inventory, weapons, etc.
/// This is an opaque handle the game layer fills.
struct GamePlayerState {
    std::string player_id;
    std::string session_id;
    bool admitted {false};
    bool connected {false};
    float last_activity_timestamp {0.0F};
};

/// Adapter that the game (Flashback) implements to translate between
/// Wish session/activity data and game state.
/// This is the ONLY allowed direction: game depends on Wish, not vice versa.
class IGameSessionAdapter {
public:
    virtual ~IGameSessionAdapter() = default;

    /// Called when a player is admitted into an activity.
    /// The game should create its internal representation of the player.
    virtual void on_player_admitted(const GamePlayerState& state) = 0;

    /// Called when a player disconnects or is removed.
    /// The game should clean up its internal state.
    virtual void on_player_removed(std::string_view player_id) = 0;

    /// Called when a match/activity completes.
    /// The game should finalize scores and report results.
    virtual void on_activity_complete(wish::core::ActivityId activity_id) = 0;

    /// Called when a heartbeat timeout is detected.
    virtual void on_player_timeout(std::string_view player_id) = 0;

    /// Called to check if a player is allowed to reconnect.
    virtual bool can_reconnect(std::string_view player_id) = 0;

    /// Get the number of currently active game players.
    virtual std::size_t active_player_count() const = 0;
};

/// Build a game-neutral match result from Wish session data.
/// The game consumes this to translate into its own match-end logic.
struct MatchReport {
    std::string activity_name;
    wish::core::ActivityId activity_id {0};
    float duration_seconds {0.0F};
    std::vector<std::string> participant_ids;
    bool was_completed {false};
    std::string summary;
};

/// Translate a Wish MatchResult into a game-neutral MatchReport.
[[nodiscard]] MatchReport build_match_report(
    wish::core::ActivityId activity_id,
    std::string_view activity_name,
    float duration_seconds,
    const std::vector<std::string>& participant_ids,
    bool was_completed,
    std::string_view summary);

} // namespace wish::integrations::flashback
