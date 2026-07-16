#include "ae/animation/character_weapon.h"

#include <cmath>

namespace ae::animation {

// ============================================================
// CharacterAnimInstance
// ============================================================

void CharacterAnimInstance::set_skeleton(int joint_count,
                                          const std::vector<int>& parent_indices) {
    joint_count_ = joint_count;
    parent_indices_ = parent_indices;
    last_pose_.resize(static_cast<std::size_t>(joint_count));
    for (auto& m : last_pose_) {
        m = skeleton::Mat4::identity();
    }
}

void CharacterAnimInstance::set_leg_ik(const IKChain& left_leg_chain,
                                        const IKChain& right_leg_chain) {
    left_leg_ik_chain_ = ik_solver_.add_chain(left_leg_chain);
    right_leg_ik_chain_ = ik_solver_.add_chain(right_leg_chain);
}

void CharacterAnimInstance::set_aim_offset_config(const AimOffsetConfig& config) {
    aim_config_ = config;
}

void CharacterAnimInstance::tick(float dt,
                                  std::vector<skeleton::Mat4>& out_matrices) {
    // Tick both state machines
    locomotion_sm_.tick(dt);
    upper_body_sm_.tick(dt);

    // The actual clip evaluation is done by the AnimationGraph (driven externally).
    // Here we just copy the last pose if it was already evaluated, or produce identity.
    out_matrices = last_pose_;
}

void CharacterAnimInstance::set_aim_offset(float yaw, float pitch) {
    aim_state_.yaw = yaw;
    aim_state_.pitch = pitch;
    aim_state_.active = true;
}

void CharacterAnimInstance::set_foot_targets(const IKTarget& left_foot,
                                              const IKTarget& right_foot,
                                              float left_weight, float right_weight) {
    if (left_leg_ik_chain_ >= 0) {
        ik_solver_.set_target(left_leg_ik_chain_, left_foot);
    }
    if (right_leg_ik_chain_ >= 0) {
        ik_solver_.set_target(right_leg_ik_chain_, right_foot);
    }
    left_foot_weight_ = left_weight;
    right_foot_weight_ = right_weight;
}

void CharacterAnimInstance::set_locomotion_blend(float speed_param) {
    locomotion_sm_.set_blend_param(speed_param);
}

void CharacterAnimInstance::set_on_ground(bool on_ground) {
    if (on_ground) {
        locomotion_sm_.trigger("landed");
    }
}

// ============================================================
// Layer helpers
// ============================================================

namespace {
// Small epsilon for float comparisons
constexpr float kLayerEpsilon = 0.0001F;
}  // namespace

// ============================================================
// Weapon animation layers
// ============================================================

void evaluate_sway_layer(WeaponAnimState& state,
                          const WeaponAnimConfig& config,
                          float dt, float look_delta_x, float look_delta_y,
                          float ads_blend,
                          skeleton::Mat4& out_offset) {
    // Effective amplitude includes ADS reduction
    float effective_amp = config.sway_amplitude *
                          (1.0F + ads_blend * (config.ads_sway_multiplier - 1.0F));

    // Sway velocity follows look delta
    state.sway_velocity_x += look_delta_x * dt * 0.5F;
    state.sway_velocity_y += look_delta_y * dt * 0.5F;

    // Damping
    float damping = config.sway_damping;
    state.sway_velocity_x *= std::exp(-damping * dt);
    state.sway_velocity_y *= std::exp(-damping * dt);

    // Accumulate phase for idle oscillation
    state.sway_phase += dt * config.sway_frequency;
    if (state.sway_phase > 1000.0F) state.sway_phase -= 1000.0F;

    float sway_x = state.sway_velocity_x +
                   std::sin(state.sway_phase) * effective_amp * 0.5F;
    float sway_y = state.sway_velocity_y +
                   std::cos(state.sway_phase * 1.3F) * effective_amp * 0.5F;

    // Compose offset: translation + subtle rotation
    out_offset = skeleton::Mat4::identity();
    out_offset = out_offset *
                 skeleton::Mat4::translation(sway_x, sway_y, 0.0F);

    float sway_rot_x = sway_y * 0.5F;
    float sway_rot_y = sway_x * 0.5F;
    float sqrt_mag = std::sqrt(sway_rot_x * sway_rot_x + sway_rot_y * sway_rot_y);
    if (sqrt_mag > kLayerEpsilon) {
        float half_angle = sqrt_mag * 0.5F;
        float s = std::sin(half_angle);
        float inv_mag = 1.0F / sqrt_mag;
        out_offset = out_offset *
                     skeleton::Mat4::rotation_quat(
                         sway_rot_x * inv_mag * s,
                         sway_rot_y * inv_mag * s,
                         0.0F,
                         std::cos(half_angle));
    }
}

void evaluate_bob_layer(WeaponAnimState& state,
                         const WeaponAnimConfig& config,
                         float dt, float player_speed, bool is_moving,
                         skeleton::Mat4& out_offset) {
    out_offset = skeleton::Mat4::identity();

    if (!is_moving || player_speed < kLayerEpsilon) {
        return;
    }

    // Frequency ramps from walk to sprint speed
    float freq = config.bob_frequency_walk;
    if (player_speed > 5.0F) {
        freq = config.bob_frequency_sprint;
    }

    float speed_factor = std::min(player_speed / 6.0F, 1.0F);
    state.bob_phase += dt * freq * speed_factor;
    if (state.bob_phase > 1000.0F) state.bob_phase -= 1000.0F;

    float bob_vert = std::sin(state.bob_phase * 2.0F) *
                     config.bob_amplitude_vertical * speed_factor;
    float bob_horiz = std::cos(state.bob_phase) *
                      config.bob_amplitude_horizontal * speed_factor;

    out_offset = out_offset *
                 skeleton::Mat4::translation(bob_horiz, bob_vert, 0.0F);

    // Subtle roll from horizontal bob
    float bob_roll = bob_horiz * 0.3F;
    float half_roll = bob_roll * 0.5F;
    out_offset = out_offset *
                 skeleton::Mat4::rotation_quat(0.0F, 0.0F,
                                              std::sin(half_roll),
                                              std::cos(half_roll));
}

void fire_weapon_kick(WeaponAnimState& state) {
    state.fire_anim_time = 0.05F;  // ~3 frames of visible kick
}

void evaluate_recoil_kick_layer(WeaponAnimState& state,
                                 const WeaponAnimConfig& config,
                                 float dt,
                                 skeleton::Mat4& out_offset) {
    out_offset = skeleton::Mat4::identity();

    if (state.fire_anim_time > 0.0F) {
        state.fire_anim_time -= dt;
        if (state.fire_anim_time < 0.0F) state.fire_anim_time = 0.0F;
    }

    float kick = (state.fire_anim_time > 0.0F)
                     ? (state.fire_anim_time / 0.05F) * 0.01F
                     : 0.0F;

    if (std::abs(kick) > kLayerEpsilon) {
        out_offset = skeleton::Mat4::translation(0.0F, -kick, 0.0F);
    }
}

// ============================================================
// Composite weapon animation (convenience wrapper)
// ============================================================

void evaluate_weapon_animation(WeaponAnimState& state,
                                const WeaponAnimConfig& config,
                                float dt, float player_speed,
                                float look_delta_x, float look_delta_y,
                                bool is_moving, bool is_ads, bool fire_pressed,
                                skeleton::Mat4& out_transform) {
    // ── ADS blend ─────────────────────────────────────────────
    float ads_target = is_ads ? 1.0F : 0.0F;
    float ads_speed = (config.ads_transition_time > 0.0F)
                          ? 1.0F / config.ads_transition_time
                          : 10.0F;
    if (state.ads_blend < ads_target) {
        state.ads_blend = std::min(state.ads_blend + ads_speed * dt, ads_target);
    } else if (state.ads_blend > ads_target) {
        state.ads_blend = std::max(state.ads_blend - ads_speed * dt, ads_target);
    }

    // ── Fire pressed triggers recoil kick ────────────────────
    if (fire_pressed) {
        fire_weapon_kick(state);
    }

    // ── Evaluate each layer ───────────────────────────────────
    skeleton::Mat4 sway_offset, bob_offset, recoil_offset;
    evaluate_sway_layer(state, config, dt, look_delta_x, look_delta_y,
                         state.ads_blend, sway_offset);
    evaluate_bob_layer(state, config, dt, player_speed, is_moving,
                        bob_offset);
    evaluate_recoil_kick_layer(state, config, dt, recoil_offset);

    // ── Compose layers: sway * bob * recoil ──────────────────
    out_transform = skeleton::Mat4::identity();
    out_transform = out_transform * sway_offset;
    out_transform = out_transform * bob_offset;
    out_transform = out_transform * recoil_offset;

    // ── ADS position (bring weapon closer) ────────────────────
    float ads_z = state.ads_blend * (-0.15F);
    float ads_y = state.ads_blend * (-0.02F);
    out_transform = out_transform *
                    skeleton::Mat4::translation(0.0F, ads_y, ads_z);
}

}  // namespace ae::animation
