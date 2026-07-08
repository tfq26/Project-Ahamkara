#pragma once

#include "ahamkara/game/net_types.h"
#include "ahamkara/game/world.h"

#include <cstdint>
#include <deque>

namespace ahamkara::game {

/**
 * @brief Manages client-side prediction and server reconciliation.
 *
 * OWNERSHIP MODEL:
 * - `world_` is a PREDICTION copy of the server's authoritative World.
 *   It runs in server mode (`is_client_ = false`) so dummies/AI are
 *   simulated identically to the server for accurate prediction.
 * - `apply_input()` feeds input to the local prediction World immediately
 *   for responsive client-side feedback.  Uses `fixed_step_seconds_` for
 *   a deterministic step size matching the server's tick rate.
 * - `reconcile()` compares the prediction against an authoritative
 *   ServerSnapshot and resets/rewinds when the two diverge beyond a
 *   threshold.
 *
 * STATE LAYERS (see headless_clients.cpp for the full three-layer model):
 *   Layer 1 — PREDICTED:  This ClientPredictionManager's local World
 *   Layer 2 — AUTHORITATIVE: ServerSnapshot::local_player (ground truth)
 *   Layer 3 — INTERPOLATED: SnapshotInterpolator lerp (smooth render)
 *
 * INTEGRATION:
 *   - Created once per network client session.
 *   - `apply_input()` is called each tick before sending the input to the
 *     server (for immediate prediction).
 *   - `reconcile()` is called when a new authoritative snapshot arrives
 *     from the server.
 *   - `world()` provides read access for state comparison / debug logging.
 */
class ClientPredictionManager {
public:
    static constexpr int kMaxPendingInputs = 128;
    static constexpr float kDefaultFixedStep = 1.0F / 60.0F;

    ClientPredictionManager();
    ~ClientPredictionManager();

    /// Apply a local input to the prediction world and buffer it for
    /// potential replay during reconciliation.  Uses the manager's
    /// internal fixed step for deterministic simulation.
    void apply_input(const PlayerInputCommand& input);

    /// Reconcile the prediction world against an authoritative server
    /// snapshot.  Resets to authoritative state and replays pending
    /// inputs if the prediction diverged beyond the error threshold.
    /// Uses the manager's internal fixed step for replay.
    void reconcile(const ServerSnapshot& snapshot);

    /// Read-only access to the prediction world for state comparison.
    [[nodiscard]] const World& world() const { return *world_; }

    /// Number of unacknowledged inputs waiting for server confirmation.
    [[nodiscard]] int pending_count() const { return static_cast<int>(pending_inputs_.size()); }

    /// The highest input sequence acknowledged by the server.
    [[nodiscard]] ae::u32 last_acknowledged() const { return last_ack_; }

    /// Set the fixed timestep used for prediction simulation.
    /// Default is 1.0/60.0.  Should match the server's tick rate.
    void set_fixed_step(float seconds) { fixed_step_seconds_ = seconds; }

    /// Get the current fixed timestep.
    [[nodiscard]] float fixed_step() const { return fixed_step_seconds_; }

    /// The current prediction tick counter.
    [[nodiscard]] ae::u32 prediction_tick() const { return prediction_tick_; }

    /// Reset the prediction world and pending inputs (e.g., on reconnect).
    void reset();

    // Non-copyable, non-movable (owns World via unique_ptr)
    ClientPredictionManager(const ClientPredictionManager&) = delete;
    ClientPredictionManager& operator=(const ClientPredictionManager&) = delete;
    ClientPredictionManager(ClientPredictionManager&&) = delete;
    ClientPredictionManager& operator=(ClientPredictionManager&&) = delete;

private:
    std::unique_ptr<World> world_;
    std::deque<PlayerInputCommand> pending_inputs_;
    ae::u32 last_ack_ {0};
    ae::u32 prediction_tick_ {0};
    float fixed_step_seconds_ {kDefaultFixedStep};
};

}  // namespace ahamkara::game
