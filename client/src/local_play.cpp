#include "ae/core/log.h"
#include "ahamkara/client/local_play.h"
#include "ae/core/math.h"
#include "ae/render/compiled_level.h"
#include <cstring>

#define AE_LOG_CATEGORY "Client"

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

    last_frame_delta_seconds_ = delta_seconds;
    fixed_timestep_.begin_frame();
    fixed_timestep_.accumulate(delta_seconds);

    const ahamkara::game::PlayerInputCommand frame_input = input_provider_->gather_input(delta_seconds);
    int steps_consumed = 0;

    while (fixed_timestep_.can_consume() && steps_consumed < fixed_timestep_.max_steps()) {
        // Save the previous fixed-step state so the renderer can interpolate
        // between stable simulation snapshots instead of variable frame deltas.
        previous_player_state_ = world_.get_player_state();
        previous_camera_anchor_ = world_.get_camera_anchor();
        std::memcpy(previous_dummies_, world_.get_dummies(), sizeof(previous_dummies_));
        has_previous_state_ = true;

        ahamkara::game::PlayerInputCommand command = frame_input;
        command.sequence = sequence_++;
        command.client_tick = ++current_tick_;
        command.client_time = total_elapsed_seconds_;

        const float fixed_step_seconds = static_cast<float>(fixed_timestep_.step());
        world_.tick(fixed_step_seconds, command);

        last_delta_seconds_ = fixed_step_seconds;
        total_elapsed_seconds_ += fixed_step_seconds;
        fixed_timestep_.consume();
        ++steps_consumed;
    }
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

float LocalPlaySimulation::get_last_frame_delta_seconds() const {
    return last_frame_delta_seconds_;
}

float LocalPlaySimulation::get_total_elapsed_seconds() const {
    return total_elapsed_seconds_;
}

ae::u32 LocalPlaySimulation::get_current_tick() const {
    return current_tick_;
}

float LocalPlaySimulation::get_interpolation_alpha() const {
    return fixed_timestep_.interpolation_alpha();
}

double LocalPlaySimulation::get_fixed_step_seconds() const {
    return fixed_timestep_.step();
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

int LocalPlaySimulation::get_reserve_ammo() const {
    return world_.get_reserve_ammo();
}

int LocalPlaySimulation::get_active_weapon_index() const {
    return world_.get_active_weapon_index();
}

bool LocalPlaySimulation::get_is_reloading() const {
    return world_.get_weapon_state().is_reloading;
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

int LocalPlaySimulation::get_interaction_attempt_count() const {
    return world_.get_interaction_attempt_count();
}

int LocalPlaySimulation::get_interaction_success_count() const {
    return world_.get_interaction_success_count();
}

int LocalPlaySimulation::get_reload_request_count() const {
    return world_.get_reload_request_count();
}

int LocalPlaySimulation::get_ability_use_count() const {
    return world_.get_ability_use_count();
}

bool LocalPlaySimulation::did_last_interaction_succeed() const {
    return world_.did_last_interaction_succeed();
}

const std::string& LocalPlaySimulation::get_last_interaction_label() const {
    return world_.get_last_interaction_label();
}

const ahamkara::game::ParticleState* LocalPlaySimulation::get_particles() const { return world_.get_particles(); }
int LocalPlaySimulation::get_particle_count() const { return world_.get_particle_count(); }
const ahamkara::game::DecalState* LocalPlaySimulation::get_decals() const { return world_.get_decals(); }
int LocalPlaySimulation::get_decal_count() const { return world_.get_decal_count(); }

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

void LocalPlaySimulation::set_interaction_targets(
    const ahamkara::game::InteractionTargetDefinition* targets,
    std::size_t count) {
    world_.set_interaction_targets(targets, count);
}

void LocalPlaySimulation::set_audio_player(ahamkara::game::IAudioPlayer* player) {
    world_.set_audio_player(player);
}

bool LocalPlaySimulation::load_level(const std::string& path) {
    ae::render::CompiledLevelLoader loader;
    ae::render::LevelAsset level;
    if (!loader.load(path, level)) {
        return false;
    }
    const bool loaded = world_.load_colliders_from_level(level);
    if (loaded) {
        has_previous_state_ = false;
        fixed_timestep_ = ae::FixedTimestepAccumulator {1.0 / 60.0};
    }
    return loaded;
}

ae::u32 LocalPlaySimulation::get_player_kills() const {
    return world_.get_player_kills();
}

ae::u32 LocalPlaySimulation::get_player_deaths() const {
    return world_.get_player_deaths();
}

float LocalPlaySimulation::get_match_time() const {
    return world_.get_match_time();
}

ae::u8 LocalPlaySimulation::get_match_phase() const {
    return world_.get_match_phase();
}

bool LocalPlaySimulation::is_match_over() const {
    return world_.is_match_over();
}

bool LocalPlaySimulation::is_player_alive() const {
    return world_.is_player_alive();
}

float LocalPlaySimulation::get_damage_feedback_timer() const {
    return world_.get_damage_feedback_timer();
}

const ahamkara::game::AbilityState& LocalPlaySimulation::get_ability_state() const {
    return world_.ability_state();
}

void LocalPlaySimulation::restart_match() {
    world_.restart_match();
    current_tick_ = 0;
    sequence_ = 0;
    total_elapsed_seconds_ = 0.0F;
    has_previous_state_ = false;
}

}  // namespace ahamkara::client
