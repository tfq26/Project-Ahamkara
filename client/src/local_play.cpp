#include "ahamkara/client/local_play.h"
#include "ae/core/math.h"
#include <cstring>

namespace ahamkara::client {

namespace {

const ahamkara::game::ParticleState kEmptyParticles[1] {};
const ahamkara::game::DecalState kEmptyDecals[1] {};

}  // namespace

// --- LocalPlaySimulation -----------------------------------------------------

LocalPlaySimulation::LocalPlaySimulation(std::unique_ptr<IInputProvider> input_provider)
    : input_provider_(std::move(input_provider)) {
}

void LocalPlaySimulation::tick(float delta_seconds) {
    if (!input_provider_) {
        return;
    }

    // Save previous state
    previous_player_state_ = world_.get_player_state();
    previous_camera_anchor_ = world_.get_camera_anchor();
    std::memcpy(previous_dummies_, world_.get_dummies(), sizeof(previous_dummies_));
    has_previous_state_ = true;

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

const ahamkara::game::TargetDummyState* LocalPlaySimulation::get_dummies() const {
    return world_.get_dummies();
}

int LocalPlaySimulation::get_dummy_count() const {
    return world_.get_dummy_count();
}

const ahamkara::game::FloatingDamageNumber* LocalPlaySimulation::get_damage_numbers() const {
    return world_.get_damage_numbers();
}

int LocalPlaySimulation::get_damage_number_count() const {
    return world_.get_damage_number_count();
}

float LocalPlaySimulation::get_hitmarker_time() const {
    return world_.get_hitmarker_time();
}

bool LocalPlaySimulation::get_hitmarker_is_critical() const {
    return world_.get_hitmarker_is_critical();
}

float LocalPlaySimulation::get_muzzle_flash_time() const {
    return world_.get_muzzle_flash_time();
}

const ahamkara::game::ParticleState* LocalPlaySimulation::get_particles() const { return kEmptyParticles; }
int LocalPlaySimulation::get_particle_count() const { return 0; }
const ahamkara::game::DecalState* LocalPlaySimulation::get_decals() const { return kEmptyDecals; }
int LocalPlaySimulation::get_decal_count() const { return 0; }

ahamkara::game::Vec3 LocalPlaySimulation::get_interpolated_player_position(float alpha) const {
    if (!has_previous_state_) {
        return world_.get_player_state().position;
    }
    const auto& curr = world_.get_player_state().position;
    const auto& prev = previous_player_state_.position;
    return {
        prev.x + (curr.x - prev.x) * alpha,
        prev.y + (curr.y - prev.y) * alpha,
        prev.z + (curr.z - prev.z) * alpha
    };
}

ahamkara::game::CameraAnchor LocalPlaySimulation::get_interpolated_camera_anchor(float alpha) const {
    if (!has_previous_state_) {
        return world_.get_camera_anchor();
    }
    const auto& curr = world_.get_camera_anchor();
    const auto& prev = previous_camera_anchor_;
    
    ahamkara::game::CameraAnchor result;
    result.position = {
        prev.position.x + (curr.position.x - prev.position.x) * alpha,
        prev.position.y + (curr.position.y - prev.position.y) * alpha,
        prev.position.z + (curr.position.z - prev.position.z) * alpha
    };
    
    float diff_yaw = ae::wrap_degrees(curr.yaw - prev.yaw);
    result.yaw = prev.yaw + diff_yaw * alpha;
    
    float diff_pitch = ae::wrap_degrees(curr.pitch - prev.pitch);
    result.pitch = prev.pitch + diff_pitch * alpha;
    
    return result;
}

ahamkara::game::TargetDummyState LocalPlaySimulation::get_interpolated_dummy(int idx, float alpha) const {
    const auto* curr_dummies = world_.get_dummies();
    if (!has_previous_state_ || idx < 0 || idx >= ahamkara::game::World::kMaxDummies) {
        return curr_dummies[idx];
    }
    
    const auto& curr = curr_dummies[idx];
    const auto& prev = previous_dummies_[idx];
    
    ahamkara::game::TargetDummyState result = curr;
    result.position = {
        prev.position.x + (curr.position.x - prev.position.x) * alpha,
        prev.position.y + (curr.position.y - prev.position.y) * alpha,
        prev.position.z + (curr.position.z - prev.position.z) * alpha
    };
    
    float diff_yaw = ae::wrap_degrees(curr.yaw - prev.yaw);
    result.yaw = prev.yaw + diff_yaw * alpha;
    
    return result;
}

void LocalPlaySimulation::set_colliders(const ahamkara::game::ColliderBox* colliders, std::size_t count) {
    world_.set_colliders(colliders, count);
}

void LocalPlaySimulation::set_audio_player(ahamkara::game::IAudioPlayer* player) {
    world_.set_audio_player(player);
}

}  // namespace ahamkara::client
