#include "ae/core/log.h"
#include "ahamkara/client/weapon_animation_controller.h"

#include <algorithm>
#include <cmath>

#define AE_LOG_CATEGORY "Client"

namespace ahamkara::client {
namespace {

constexpr float kPi = 3.14159265358979323846F;

/// Smooth Hermite interpolation: smoothstep from 0 to 1 over [edge0, edge1].
inline float smoothstep(float edge0, float edge1, float x) {
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

} // namespace

WeaponAnimationController::WeaponAnimationController() {
    // Register weapon profiles
    profiles_[kArWeaponIndex] = make_ar15_profile();
    profiles_[kShotgunWeaponIndex] = make_shotgun_profile();
    profiles_[kRlWeaponIndex] = make_rl_profile();
    reset();
}

WeaponAnimProfile WeaponAnimationController::make_ar15_profile() {
    WeaponAnimProfile p;
    p.anim_config.sway_amplitude = 0.0075F;
    p.anim_config.sway_frequency = 1.35F;
    p.anim_config.sway_damping = 4.0F;
    p.anim_config.bob_amplitude_vertical = 0.010F;
    p.anim_config.bob_amplitude_horizontal = 0.005F;
    p.anim_config.bob_frequency_walk = 2.2F;
    p.anim_config.bob_frequency_sprint = 3.75F;
    p.anim_config.ads_transition_time = 0.18F;
    p.anim_config.ads_sway_multiplier = 0.28F;
    p.anim_config.recoil.kick_pitch_min = 0.025F;
    p.anim_config.recoil.kick_pitch_max = 0.045F;
    p.anim_config.recoil.kick_yaw_min = -0.012F;
    p.anim_config.recoil.kick_yaw_max = 0.012F;
    p.anim_config.recoil.kick_roll_min = -0.006F;
    p.anim_config.recoil.kick_roll_max = 0.006F;
    p.anim_config.recoil.pattern_scale = 1.0F;
    p.anim_config.recoil.vertical_bias = 0.82F;
    p.anim_config.recoil.randomness = 0.12F;
    p.anim_config.recoil.recovery_speed = 6.5F;
    p.anim_config.recoil.recovery_damping = 0.82F;
    p.anim_config.recoil.ads_kick_multiplier = 0.6F;
    p.anim_config.recoil.recoil_joint = 0;
    p.reload_duration = 2.0F;
    p.melee_duration = 0.6F;
    p.melee_reach = 1.5F;
    p.melee_damage = 35.0F;
    return p;
}

WeaponAnimProfile WeaponAnimationController::make_shotgun_profile() {
    WeaponAnimProfile p;
    p.anim_config.sway_amplitude = 0.012F;
    p.anim_config.sway_frequency = 1.1F;
    p.anim_config.sway_damping = 3.5F;
    p.anim_config.bob_amplitude_vertical = 0.015F;
    p.anim_config.bob_amplitude_horizontal = 0.008F;
    p.anim_config.bob_frequency_walk = 1.8F;
    p.anim_config.bob_frequency_sprint = 3.2F;
    p.anim_config.ads_transition_time = 0.25F;
    p.anim_config.ads_sway_multiplier = 0.35F;
    p.anim_config.recoil.kick_pitch_min = 0.080F;
    p.anim_config.recoil.kick_pitch_max = 0.120F;
    p.anim_config.recoil.kick_yaw_min = -0.025F;
    p.anim_config.recoil.kick_yaw_max = 0.025F;
    p.anim_config.recoil.kick_roll_min = -0.015F;
    p.anim_config.recoil.kick_roll_max = 0.015F;
    p.anim_config.recoil.pattern_scale = 1.0F;
    p.anim_config.recoil.vertical_bias = 0.75F;
    p.anim_config.recoil.randomness = 0.18F;
    p.anim_config.recoil.recovery_speed = 4.0F;
    p.anim_config.recoil.recovery_damping = 0.70F;
    p.anim_config.recoil.ads_kick_multiplier = 0.7F;
    p.anim_config.recoil.recoil_joint = 0;
    p.reload_duration = 3.5F;
    p.melee_duration = 0.7F;
    p.melee_reach = 1.6F;
    p.melee_damage = 40.0F;
    return p;
}

WeaponAnimProfile WeaponAnimationController::make_rl_profile() {
    WeaponAnimProfile p;
    p.anim_config.sway_amplitude = 0.010F;
    p.anim_config.sway_frequency = 0.9F;
    p.anim_config.sway_damping = 3.0F;
    p.anim_config.bob_amplitude_vertical = 0.012F;
    p.anim_config.bob_amplitude_horizontal = 0.006F;
    p.anim_config.bob_frequency_walk = 1.6F;
    p.anim_config.bob_frequency_sprint = 2.8F;
    p.anim_config.ads_transition_time = 0.30F;
    p.anim_config.ads_sway_multiplier = 0.30F;
    p.anim_config.recoil.kick_pitch_min = 0.050F;
    p.anim_config.recoil.kick_pitch_max = 0.090F;
    p.anim_config.recoil.kick_yaw_min = -0.018F;
    p.anim_config.recoil.kick_yaw_max = 0.018F;
    p.anim_config.recoil.kick_roll_min = -0.010F;
    p.anim_config.recoil.kick_roll_max = 0.010F;
    p.anim_config.recoil.pattern_scale = 1.0F;
    p.anim_config.recoil.vertical_bias = 0.78F;
    p.anim_config.recoil.randomness = 0.15F;
    p.anim_config.recoil.recovery_speed = 5.0F;
    p.anim_config.recoil.recovery_damping = 0.75F;
    p.anim_config.recoil.ads_kick_multiplier = 0.65F;
    p.anim_config.recoil.recoil_joint = 0;
    p.reload_duration = 2.8F;
    p.reload_data = weapon_reload_data(kRlWeaponIndex);
    p.melee_duration = 0.65F;
    p.melee_reach = 1.5F;
    p.melee_damage = 30.0F;
    return p;
}

void WeaponAnimationController::reset() {
    active_weapon_index_ = -1;
    has_transform_ = false;
    reload_active_ = false;
    reload_timer_ = 0.0F;
    reload_phase_ = ReloadPhase::Idle;
    reload_normalized_ = 0.0F;
    reload_ik_offset_[0] = 0.0F;
    reload_ik_offset_[1] = 0.0F;
    reload_ik_offset_[2] = 0.0F;
    melee_active_ = false;
    melee_timer_ = 0.0F;
    melee_phase_ = 0;
    anim_state_ = {};
    transform_.fill(0.0F);
    transform_[0] = 1.0F;
    transform_[5] = 1.0F;
    transform_[10] = 1.0F;
    transform_[15] = 1.0F;
}

float WeaponAnimationController::horizontal_speed(const ahamkara::game::Vec3& velocity) {
    return std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
}

static ae::skeleton::Mat4 make_axis_angle_rotation(float x, float y, float z, float degrees) {
    const float radians = degrees * (kPi / 180.0F);
    const float half = radians * 0.5F;
    const float s = std::sin(half);
    return ae::skeleton::Mat4::rotation_quat(x * s, y * s, z * s, std::cos(half));
}

void WeaponAnimationController::tick(float dt,
                                     const ClientSimulationSnapshot& snapshot,
                                     const ahamkara::game::PlayerInputCommand& input) {
    const int weapon_index = snapshot.weapon_index;
    if (weapon_index != active_weapon_index_) {
        active_weapon_index_ = weapon_index;
        reload_active_ = false;
        reload_timer_ = 0.0F;
        melee_active_ = false;
        melee_timer_ = 0.0F;
        melee_phase_ = 0;
        anim_state_ = {};
    }

    has_transform_ = false;
    if (profiles_.find(weapon_index) == profiles_.end()) {
        return; // unregistered weapon, no animation override
    }

    update_weapon(dt, snapshot, input);
    has_transform_ = true;
}

bool WeaponAnimationController::trigger_melee() {
    if (melee_active_)
        return false;
    melee_active_ = true;
    melee_timer_ = melee_duration_;
    melee_phase_ = 1;
    return true;
}

float WeaponAnimationController::melee_normalized() const {
    if (melee_duration_ <= 0.0F)
        return 0.0F;
    return std::clamp(1.0F - (melee_timer_ / melee_duration_), 0.0F, 1.0F);
}

void WeaponAnimationController::notify_fired() {
    ae::animation::fire_weapon_kick(anim_state_);
}

void WeaponAnimationController::update_weapon(float dt,
                                              const ClientSimulationSnapshot& snapshot,
                                              const ahamkara::game::PlayerInputCommand& input) {
    const auto& profile = profiles_[active_weapon_index_];
    const float speed = horizontal_speed(snapshot.player_state.velocity);
    const bool is_moving = speed > 0.1F;

    // ── ADS blend ─────────────────────────────────────────────
    float ads_target = input.aim_held ? 1.0F : 0.0F;
    float ads_speed = (profile.anim_config.ads_transition_time > 0.0F)
                          ? 1.0F / profile.anim_config.ads_transition_time
                          : 10.0F;
    if (anim_state_.ads_blend < ads_target) {
        anim_state_.ads_blend = std::min(
            anim_state_.ads_blend + ads_speed * dt, ads_target);
    } else if (anim_state_.ads_blend > ads_target) {
        anim_state_.ads_blend = std::max(
            anim_state_.ads_blend - ads_speed * dt, ads_target);
    }

    // ── Evaluate each animation layer ─────────────────────────
    ae::skeleton::Mat4 sway_offset, bob_offset, recoil_offset;
    ae::animation::evaluate_sway_layer(
        anim_state_, profile.anim_config, dt,
        input.look_delta.x, input.look_delta.y,
        anim_state_.ads_blend, sway_offset);
    ae::animation::evaluate_bob_layer(
        anim_state_, profile.anim_config, dt,
        speed, is_moving, bob_offset);
    ae::animation::evaluate_recoil_kick_layer(
        anim_state_, profile.anim_config, dt, recoil_offset);

    // ── Compose layers: sway * bob * recoil ──────────────────
    ae::skeleton::Mat4 local = ae::skeleton::Mat4::identity();
    local = local * sway_offset;
    local = local * bob_offset;
    local = local * recoil_offset;

    // Note: ADS position offset is handled by the per-weapon viewmodel
    // offset blend in client_frame_pipeline.cpp (hip + ADS transform data).
    // The animation controller handles only procedural motion (sway, bob, recoil).
    // See WeaponAdsTransform/kWeaponAdsTransforms in weapon_viewmodel_data.h.

    // --- Reload (phase-driven animation) ---
    // Phase timing:
    //   GrabMag:      hand moves from grip to magazine position
    //   RemoveMag:    hand holds magazine, weapon tilts to expose magwell
    //   InsertMag:    hand inserts new magazine, weapon returns from tilt
    //   ReturnToGrip: hand moves back from magazine to grip
    if (!reload_active_ && input.reload_pressed && snapshot.ammo_current < snapshot.ammo_max && snapshot.reserve_ammo > 0) {
        reload_active_ = true;
        reload_timer_ = profile.reload_duration;
        reload_phase_ = ReloadPhase::GrabMag;
        reload_normalized_ = 0.0F;
        anim_state_.reload_anim_time = profile.reload_duration;
        anim_state_.is_reloading = true;
    }

    if (reload_active_) {
        // --- Phase computation ---
        reload_timer_ = std::max(0.0F, reload_timer_ - dt);
        reload_normalized_ = 1.0F - (reload_timer_ / profile.reload_duration);

        const auto& rd = profile.reload_data;

        // Determine current phase from normalized progress.
        if (reload_normalized_ < rd.grab_start) {
            reload_phase_ = ReloadPhase::Idle;
        } else if (reload_normalized_ < rd.grab_end) {
            reload_phase_ = ReloadPhase::GrabMag;
        } else if (reload_normalized_ < rd.remove_end) {
            reload_phase_ = ReloadPhase::RemoveMag;
        } else if (reload_normalized_ < rd.insert_end) {
            reload_phase_ = ReloadPhase::InsertMag;
        } else if (reload_normalized_ < rd.return_end) {
            reload_phase_ = ReloadPhase::ReturnToGrip;
        } else {
            reload_phase_ = ReloadPhase::Idle;
        }

        // --- IK offset: hand moves from grip (0,0,0) to magazine position ---
        // During GrabMag: lerp IK offset from (0,0,0) to magazine position
        // During RemoveMag: hold at magazine position
        // During InsertMag: hold at magazine position
        // During ReturnToGrip: lerp IK offset back to (0,0,0)
        if (reload_phase_ == ReloadPhase::GrabMag) {
            const float t = smoothstep(rd.grab_start, rd.grab_end, reload_normalized_);
            reload_ik_offset_[0] = rd.mag_pos_x * t;
            reload_ik_offset_[1] = rd.mag_pos_y * t;
            reload_ik_offset_[2] = rd.mag_pos_z * t;
        } else if (reload_phase_ == ReloadPhase::RemoveMag || reload_phase_ == ReloadPhase::InsertMag) {
            // Hold hand at magazine position during these phases
            reload_ik_offset_[0] = rd.mag_pos_x;
            reload_ik_offset_[1] = rd.mag_pos_y;
            reload_ik_offset_[2] = rd.mag_pos_z;
        } else if (reload_phase_ == ReloadPhase::ReturnToGrip) {
            const float t = 1.0F - smoothstep(rd.return_start, rd.return_end, reload_normalized_);
            reload_ik_offset_[0] = rd.mag_pos_x * t;
            reload_ik_offset_[1] = rd.mag_pos_y * t;
            reload_ik_offset_[2] = rd.mag_pos_z * t;
        } else {
            reload_ik_offset_[0] = 0.0F;
            reload_ik_offset_[1] = 0.0F;
            reload_ik_offset_[2] = 0.0F;
        }

        // --- Weapon tilt: pitch up to expose magwell ---
        // Tilt ramps up during GrabMag, holds through RemoveMag/InsertMag, fades in ReturnToGrip.
        float tilt_weight = 0.0F;
        if (reload_phase_ == ReloadPhase::GrabMag) {
            tilt_weight = smoothstep(rd.grab_start, rd.grab_end, reload_normalized_);
        } else if (reload_phase_ == ReloadPhase::RemoveMag || reload_phase_ == ReloadPhase::InsertMag) {
            tilt_weight = 1.0F;
        } else if (reload_phase_ == ReloadPhase::ReturnToGrip) {
            tilt_weight = 1.0F - smoothstep(rd.return_start, rd.return_end, reload_normalized_);
        }

        // Apply position offset and tilt rotation
        ae::skeleton::Mat4 reload_pose = ae::skeleton::Mat4::identity();
        reload_pose = reload_pose * ae::skeleton::Mat4::translation(
                                        rd.offset_right * tilt_weight,
                                        rd.offset_up * tilt_weight,
                                        rd.offset_forward * tilt_weight);
        reload_pose = reload_pose * make_axis_angle_rotation(1.0F, 0.0F, 0.0F,
                                                             rd.tilt_pitch_deg * tilt_weight);
        reload_pose = reload_pose * make_axis_angle_rotation(0.0F, 1.0F, 0.0F,
                                                             rd.tilt_yaw_deg * tilt_weight);
        reload_pose = reload_pose * make_axis_angle_rotation(0.0F, 0.0F, 1.0F,
                                                             rd.tilt_roll_deg * tilt_weight);

        local = local * reload_pose;

        if (reload_timer_ <= 0.0F) {
            reload_active_ = false;
            reload_phase_ = ReloadPhase::Idle;
            reload_normalized_ = 0.0F;
            reload_ik_offset_[0] = 0.0F;
            reload_ik_offset_[1] = 0.0F;
            reload_ik_offset_[2] = 0.0F;
            anim_state_.is_reloading = false;
            anim_state_.reload_anim_time = 0.0F;
        }
    } else {
        reload_phase_ = ReloadPhase::Idle;
        reload_normalized_ = 0.0F;
        reload_ik_offset_[0] = 0.0F;
        reload_ik_offset_[1] = 0.0F;
        reload_ik_offset_[2] = 0.0F;
        anim_state_.is_reloading = false;
        anim_state_.reload_anim_time = 0.0F;
    }

    // --- Melee (externally triggered via trigger_melee()) ---
    if (melee_active_) {
        melee_timer_ = std::max(0.0F, melee_timer_ - dt);
        const float t = melee_normalized();

        // Three-phase melee: wind-up (0-0.3), strike (0.3-0.7), recover (0.7-1.0)
        if (t < 0.3F) {
            melee_phase_ = 1;
        } else if (t < 0.7F) {
            melee_phase_ = 2;
        } else {
            melee_phase_ = 3;
        }

        ae::skeleton::Mat4 melee_pose = ae::skeleton::Mat4::identity();
        if (melee_phase_ == 1) {
            const float p = t / 0.3F;
            melee_pose = melee_pose * ae::skeleton::Mat4::translation(-0.01F * p, 0.0F, -0.08F * p);
            melee_pose = melee_pose * make_axis_angle_rotation(0.0F, 0.0F, 1.0F, -15.0F * p);
            melee_pose = melee_pose * make_axis_angle_rotation(1.0F, 0.0F, 0.0F, -10.0F * p);
        } else if (melee_phase_ == 2) {
            const float p = (t - 0.3F) / 0.4F;
            melee_pose = melee_pose * ae::skeleton::Mat4::translation(-0.01F + 0.06F * p, 0.0F, -0.08F + 0.15F * p);
            melee_pose = melee_pose * make_axis_angle_rotation(0.0F, 0.0F, 1.0F, -15.0F + 25.0F * p);
            melee_pose = melee_pose * make_axis_angle_rotation(1.0F, 0.0F, 0.0F, -10.0F + 15.0F * p);
        } else if (melee_phase_ == 3) {
            const float p = (t - 0.7F) / 0.3F;
            const float ease = 1.0F - p;
            melee_pose = melee_pose * ae::skeleton::Mat4::translation(0.05F * ease, 0.0F, 0.07F * ease);
            melee_pose = melee_pose * make_axis_angle_rotation(0.0F, 0.0F, 1.0F, 10.0F * ease);
            melee_pose = melee_pose * make_axis_angle_rotation(1.0F, 0.0F, 0.0F, 5.0F * ease);
        }

        local = local * melee_pose;

        if (melee_timer_ <= 0.0F) {
            melee_active_ = false;
            melee_phase_ = 0;
        }
    }

    transform_ = local.m;
}

} // namespace ahamkara::client
