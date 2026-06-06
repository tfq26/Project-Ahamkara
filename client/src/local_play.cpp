#include "ahamkara/client/local_play.h"

namespace ahamkara::client {

// --- LocalPlaySimulation -----------------------------------------------------

LocalPlaySimulation::LocalPlaySimulation(std::unique_ptr<IInputProvider> input_provider)
    : input_provider_(std::move(input_provider)) {
}

void LocalPlaySimulation::tick(float delta_seconds) {
    if (!input_provider_) {
        return;
    }

    // Gather input
    ahamkara::game::PlayerInputCommand command = input_provider_->gather_input(delta_seconds);

    // Populate frame sequencing and timing fields
    command.sequence = sequence_++;
    command.client_tick = ++current_tick_;
    command.client_time = total_elapsed_seconds_;

    // Tick the shared game world simulation
    world_.tick(delta_seconds, command);

    // Record per-frame timing metrics
    last_delta_seconds_ = delta_seconds;
    total_elapsed_seconds_ += delta_seconds;
}

const ahamkara::game::ReplicatedPlayerState& LocalPlaySimulation::get_player_state() const {
    return world_.get_player_state();
}

const ahamkara::game::CameraAnchor& LocalPlaySimulation::get_camera_anchor() const {
    return world_.get_camera_anchor();
}

float LocalPlaySimulation::get_player_visual_height() const {
    return world_.get_player_visual_height();
}

float LocalPlaySimulation::get_last_delta_seconds() const {
    return last_delta_seconds_;
}

float LocalPlaySimulation::get_total_elapsed_seconds() const {
    return total_elapsed_seconds_;
}

ae::u32 LocalPlaySimulation::get_current_tick() const {
    return current_tick_;
}

const ahamkara::game::ProjectileState* LocalPlaySimulation::get_projectiles() const {
    return world_.get_projectiles();
}

int LocalPlaySimulation::get_projectile_count() const {
    return world_.get_projectile_count();
}

int LocalPlaySimulation::get_ammo_current() const {
    return world_.get_ammo_current();
}

int LocalPlaySimulation::get_ammo_max() const {
    return world_.get_ammo_max();
}

}  // namespace ahamkara::client
