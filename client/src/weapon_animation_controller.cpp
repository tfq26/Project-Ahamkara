#include "ae/core/log.h"
#include "ahamkara/client/weapon_animation_controller.h"

#include <algorithm>
#include <cmath>

#define AE_LOG_CATEGORY "Client"

namespace ahamkara::client {
namespace {

constexpr float kPi = 3.14159265358979323846F;

}  // namespace

WeaponAnimationController::WeaponAnimationController() {
    ar_config_.sway_amplitude = 0.0075F;
    ar_config_.sway_frequency = 1.35F;
    ar_config_.sway_damping = 4.0F;
    ar_config_.bob_amplitude_vertical = 0.010F;
    ar_config_.bob_amplitude_horizontal = 0.005F;
    ar_config_.bob_frequency_walk = 2.2F;
    ar_config_.bob_frequency_sprint = 3.75F;
    ar_config_.ads_transition_time = 0.18F;
    ar_config_.ads_sway_multiplier = 0.28F;
    ar_config_.recoil.kick_pitch_min = 0.025F;
    ar_config_.recoil.kick_pitch_max = 0.045F;
    ar_config_.recoil.kick_yaw_min = -0.012F;
    ar_config_.recoil.kick_yaw_max = 0.012F;
    ar_config_.recoil.kick_roll_min = -0.006F;
    ar_config_.recoil.kick_roll_max = 0.006F;
    ar_config_.recoil.pattern_scale = 1.0F;
    ar_config_.recoil.vertical_bias = 0.82F;
    ar_config_.recoil.randomness = 0.12F;
    ar_config_.recoil.recovery_speed = 6.5F;
    ar_config_.recoil.recovery_damping = 0.82F;
    ar_config_.recoil.ads_kick_multiplier = 0.6F;
    ar_config_.recoil.recoil_joint = 0;

    reset();
}

void WeaponAnimationController::reset() {
    active_weapon_index_ = -1;
    has_transform_ = false;
    reload_active_ = false;
    reload_timer_ = 0.0F;
    ar_state_ = {};
    transform_.fill(0.0F);
    transform_[0] = 1.0F;
    transform_[5] = 1.0F;
    transform_[10] = 1.0F;
    transform_[15] = 1.0F;
}

float WeaponAnimationController::horizontal_speed(const ahamkara::game::Vec3& velocity) {
    return std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
}

ae::render::Mat4 WeaponAnimationController::axis_angle_rotation(float x, float y, float z, float degrees) {
    const float radians = degrees * (kPi / 180.0F);
    const float half = radians * 0.5F;
    const float s = std::sin(half);
    return ae::render::Mat4::rotation_quat(x * s, y * s, z * s, std::cos(half));
}

void WeaponAnimationController::tick(float dt,
                                      const ClientSimulationSnapshot& snapshot,
                                      const ahamkara::game::PlayerInputCommand& input) {
    const int weapon_index = snapshot.weapon_index;
    if (weapon_index != active_weapon_index_) {
        active_weapon_index_ = weapon_index;
        reload_active_ = false;
        reload_timer_ = 0.0F;
        ar_state_ = {};
    }

    has_transform_ = false;
    if (weapon_index != kArWeaponIndex) {
        return;
    }

    update_ar15(dt, snapshot, input);
    has_transform_ = true;
}

void WeaponAnimationController::update_ar15(float dt,
                                            const ClientSimulationSnapshot& snapshot,
                                            const ahamkara::game::PlayerInputCommand& input) {
    const float speed = horizontal_speed(snapshot.player_state.velocity);
    const bool is_moving = speed > 0.1F;

    ae::render::Mat4 local = ae::render::Mat4::identity();
    evaluate_weapon_animation(
        ar_state_, ar_config_, dt, speed,
        input.look_delta.x, input.look_delta.y,
        is_moving, false, input.fire_held, local);

    if (!reload_active_
        && input.reload_pressed
        && snapshot.ammo_current < snapshot.ammo_max
        && snapshot.reserve_ammo > 0) {
        reload_active_ = true;
        reload_timer_ = kArReloadSeconds;
        ar_state_.reload_anim_time = kArReloadSeconds;
        ar_state_.is_reloading = true;
    }

    if (reload_active_) {
        reload_timer_ = std::max(0.0F, reload_timer_ - dt);
        const float reload_blend = 1.0F - (reload_timer_ / kArReloadSeconds);
        const float settle = std::sin(std::clamp(reload_blend, 0.0F, 1.0F) * kPi);

        ae::render::Mat4 reload_pose = ae::render::Mat4::identity();
        reload_pose = reload_pose * ae::render::Mat4::translation(0.02F * settle, -0.09F * settle, -0.11F * settle);
        reload_pose = reload_pose * axis_angle_rotation(1.0F, 0.0F, 0.0F, -22.0F * settle);
        reload_pose = reload_pose * axis_angle_rotation(0.0F, 1.0F, 0.0F, 8.0F * settle);
        reload_pose = reload_pose * axis_angle_rotation(0.0F, 0.0F, 1.0F, -4.0F * settle);

        local = local * reload_pose;

        if (reload_timer_ <= 0.0F) {
            reload_active_ = false;
            ar_state_.is_reloading = false;
            ar_state_.reload_anim_time = 0.0F;
        }
    } else {
        ar_state_.is_reloading = false;
        ar_state_.reload_anim_time = 0.0F;
    }

    transform_ = local.m;
}

}  // namespace ahamkara::client
