#include "ahamkara/game/client_prediction.h"

#include <algorithm>
#include <cmath>

namespace ahamkara::game {

ClientPredictionManager::ClientPredictionManager()
    : world_(std::make_unique<World>()) {
    // The prediction world is a visual copy — client-side effects only.
    world_->set_is_client(true);
}

ClientPredictionManager::~ClientPredictionManager() = default;

void ClientPredictionManager::apply_input(
    const PlayerInputCommand& input, float delta_seconds) {
    // Apply input to the local prediction world for responsive feedback.
    world_->tick(delta_seconds, input);

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

        // Replay all unacknowledged inputs on the corrected state. The pending
        // queue already had server-acknowledged inputs removed above, so it
        // holds exactly the unacked inputs and is correct to replay on every
        // snapshot — including the first one. (Previously a `last_ack_ != 0`
        // guard skipped replay on the first snapshot, dropping pre-snapshot
        // inputs; that Phase 4 gap is now closed.)
        constexpr float kFixedStep = 1.0F / 60.0F;
        for (const auto& pending : pending_inputs_) {
            world_->tick(kFixedStep, pending);
        }
    }
}

void ClientPredictionManager::reset() {
    world_ = std::make_unique<World>();
    world_->set_is_client(true);
    pending_inputs_.clear();
    last_ack_ = 0;
}

}  // namespace ahamkara::game
