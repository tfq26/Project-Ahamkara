#include "ahamkara/game/client_prediction.h"

#include <algorithm>
#include <cmath>

namespace ahamkara::game {

ClientPredictionManager::ClientPredictionManager()
    : world_(std::make_unique<World>()) {
    // The prediction world runs in server mode so it simulates dummies,
    // AI, and projectiles identically to the server.  This keeps the
    // predicted state consistent with what the server will confirm.
    world_->set_is_client(false);
}

ClientPredictionManager::~ClientPredictionManager() = default;

void ClientPredictionManager::apply_input(
    const PlayerInputCommand& input) {
    // Advance the simulation by one fixed step, then apply the input.
    // This mirrors the server's tick() path: advance_sim + apply_input.
    world_->advance_sim(fixed_step_seconds_);
    world_->apply_input(fixed_step_seconds_, input);

    ++prediction_tick_;

    // Buffer input for potential replay during reconciliation.
    pending_inputs_.push_back(input);
    while (static_cast<int>(pending_inputs_.size()) > kMaxPendingInputs) {
        pending_inputs_.pop_front();
    }
}

void ClientPredictionManager::reconcile(const ServerSnapshot& snapshot) {
    // Discard inputs the server has already processed.
    while (!pending_inputs_.empty() &&
           pending_inputs_.front().sequence <= snapshot.last_processed_input) {
        pending_inputs_.pop_front();
    }

    last_ack_ = snapshot.last_processed_input;

    const auto& authoritative = snapshot.local_player;
    const auto& predicted = world_->get_player_state();

    // Position error threshold for reconciliation (in world units).
    constexpr float kReconcileThreshold = 0.05F;

    float dx = predicted.position.x - authoritative.position.x;
    float dy = predicted.position.y - authoritative.position.y;
    float dz = predicted.position.z - authoritative.position.z;
    float error = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (error > kReconcileThreshold) {
        // Reset prediction world to authoritative state.
        world_->set_player_state(authoritative);

        // Replay all unacknowledged inputs on the corrected state, using
        // the manager's fixed step (not a hardcoded value).  The pending
        // queue already had server-acknowledged inputs removed above, so
        // it holds exactly the unacked inputs.
        const float step = fixed_step_seconds_;
        for (const auto& pending : pending_inputs_) {
            world_->advance_sim(step);
            world_->apply_input(step, pending);
        }
    }
}

ServerSnapshot ClientPredictionManager::capture_prediction_state() const {
    ServerSnapshot snap;
    snap.server_tick = prediction_tick_;
    snap.last_processed_input = last_ack_;
    snap.local_player = world_->get_player_state();

    // Dummies
    const int dc = world_->get_dummy_count();
    snap.dummy_count = static_cast<ae::u8>(std::min(dc, 4));
    for (ae::u8 i = 0; i < snap.dummy_count; ++i) {
        snap.dummies[i] = world_->get_dummies()[i];
    }

    // Projectiles
    const int pc = world_->get_projectile_count();
    snap.projectile_count = static_cast<ae::u8>(std::min(pc, 8));
    for (ae::u8 i = 0; i < snap.projectile_count; ++i) {
        snap.projectiles[i] = world_->get_projectiles()[i];
    }

    // Match state
    snap.match_time = world_->get_match_time();
    snap.match_phase = world_->get_match_phase();
    snap.team_score_red = world_->get_team_score_red();
    snap.team_score_blue = world_->get_team_score_blue();

    return snap;
}

void ClientPredictionManager::replay_buffered_inputs(World& target, ae::u32 last_ack) const {
    const float step = fixed_step_seconds_;
    for (const auto& pending : pending_inputs_) {
        if (pending.sequence > last_ack) {
            target.advance_sim(step);
            target.apply_input(step, pending);
        }
    }
}

void ClientPredictionManager::reset() {
    world_ = std::make_unique<World>();
    world_->set_is_client(false);
    pending_inputs_.clear();
    last_ack_ = 0;
    prediction_tick_ = 0;
}

}  // namespace ahamkara::game
