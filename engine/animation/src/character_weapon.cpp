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
        m = render::Mat4::identity();
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
                                  std::vector<render::Mat4>& out_matrices) {
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
// Weapon animation
// ============================================================

void evaluate_weapon_animation(WeaponAnimState& state,
                                const WeaponAnimConfig& config,
                                float dt, float player_speed,
                                float look_delta_x, float look_delta_y,
                                bool is_moving, bool is_ads, bool fire_pressed,
                                render::Mat4& out_transform) {
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

    // ── Idle sway ─────────────────────────────────────────────
    float effective_sway_amp = config.sway_amplitude *
                               (1.0F + state.ads_blend * (config.ads_sway_multiplier - 1.0F));

    // Sway velocity follows look delta
    state.sway_velocity_x += look_delta_x * dt * 0.5F;
    state.sway_velocity_y += look_delta_y * dt * 0.5F;

    // Damping
    float damping = config.sway_damping;
    state.sway_velocity_x *= std::exp(-damping * dt);
    state.sway_velocity_y *= std::exp(-damping * dt);

    // Accumulate phase
    state.sway_phase += dt * config.sway_frequency;
    if (state.sway_phase > 1000.0F) state.sway_phase -= 1000.0F;

    float sway_x = state.sway_velocity_x + std::sin(state.sway_phase) * effective_sway_amp * 0.5F;
    float sway_y = state.sway_velocity_y + std::cos(state.sway_phase * 1.3F) * effective_sway_amp * 0.5F;

    // ── Movement bob ──────────────────────────────────────────
    float bob_freq = config.bob_frequency_walk;
    if (player_speed > 5.0F) bob_freq = config.bob_frequency_sprint;

    float bob_speed_factor = std::min(player_speed / 6.0F, 1.0F);
    state.bob_phase += dt * bob_freq * bob_speed_factor;
    if (state.bob_phase > 1000.0F) state.bob_phase -= 1000.0F;

    float bob_vertical = 0.0F;
    float bob_horizontal = 0.0F;
    if (is_moving) {
        bob_vertical = std::sin(state.bob_phase * 2.0F) * config.bob_amplitude_vertical * bob_speed_factor;
        bob_horizontal = std::cos(state.bob_phase) * config.bob_amplitude_horizontal * bob_speed_factor;
    }

    // ── Fire animation ───────────────────────────────────────
    if (fire_pressed) {
        state.fire_anim_time = 0.05F;  // ~3 frames kick
    }
    if (state.fire_anim_time > 0.0F) {
        state.fire_anim_time -= dt;
        if (state.fire_anim_time < 0.0F) state.fire_anim_time = 0.0F;
    }
    float fire_kick = (state.fire_anim_time > 0.0F)
                          ? state.fire_anim_time / 0.05F * 0.01F
                          : 0.0F;

    // ── Compose final transform ──────────────────────────────

    // Start with identity
    out_transform = render::Mat4::identity();

    // Apply translation: sway + bob + ADS position (bring weapon closer)
    float ads_translate_z = state.ads_blend * (-0.15F);  // pull weapon closer in ADS
    float ads_translate_y = state.ads_blend * (-0.02F);  // slight drop for sight alignment

    out_transform = out_transform *
                    render::Mat4::translation(
                        sway_x + bob_horizontal,
                        sway_y + bob_vertical + ads_translate_y - fire_kick,
                        ads_translate_z);

    // Apply subtle rotation from sway
    float sway_rot_x = sway_y * 0.5F;
    float sway_rot_y = sway_x * 0.5F;
    float sway_rot_z = bob_horizontal * 0.3F;

    float half_x = std::sin(sway_rot_x * 0.5F);
    float half_y = std::sin(sway_rot_y * 0.5F);
    float half_z = std::sin(sway_rot_z * 0.5F);
    float half_w = std::cos(std::sqrt(sway_rot_x * sway_rot_x +
                                       sway_rot_y * sway_rot_y +
                                       sway_rot_z * sway_rot_z) * 0.5F);

    out_transform = out_transform *
                    render::Mat4::rotation_quat(half_x, half_y, half_z, half_w);
}

}  // namespace ae::animation
