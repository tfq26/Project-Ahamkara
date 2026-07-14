#include "ae/core/log.h"
#include "ae/animation/animation_driver.h"

#include <cmath>


#define AE_LOG_CATEGORY "Animation"

namespace ae::animation {

void AnimationDriver::init_locomotion(StateMachine& sm) {
    // Set up a standard FPS locomotion state machine.
    // These clip names are placeholders — real clips come from glTF assets.

    sm.add_state("idle", "anim_idle");
    sm.add_state("walk", "anim_walk");
    sm.add_state("sprint", "anim_sprint");
    sm.add_state("jump", "anim_jump");
    sm.add_state("slide", "anim_slide");
    sm.add_state("crouch", "anim_crouch_idle");
    sm.add_state("crouch_walk", "anim_crouch_walk");
    sm.add_state("land", "anim_land");

    // Idle ↔ Walk ↔ Sprint
    sm.add_transition("idle", "walk", "start_moving", 0.15F);
    sm.add_transition("walk", "idle", "stop_moving", 0.2F);
    sm.add_transition("walk", "sprint", "start_sprinting", 0.15F);
    sm.add_transition("sprint", "walk", "stop_sprinting", 0.25F);
    sm.add_transition("sprint", "idle", "stop_moving", 0.2F);

    // Jump
    sm.add_transition("idle", "jump", "jump", 0.1F);
    sm.add_transition("walk", "jump", "jump", 0.1F);
    sm.add_transition("sprint", "jump", "jump", 0.1F);

    // Land → idle
    sm.add_exit_time_transition("jump", "land", 0.9F, 0.1F);
    sm.add_exit_time_transition("land", "idle", 0.95F, 0.15F);

    // Slide
    sm.add_transition("sprint", "slide", "slide", 0.08F);
    sm.add_exit_time_transition("slide", "idle", 0.95F, 0.15F);

    // Crouch
    sm.add_transition("idle", "crouch", "crouch", 0.2F);
    sm.add_transition("walk", "crouch_walk", "crouch", 0.2F);
    sm.add_transition("crouch", "idle", "uncrouch", 0.2F);
    sm.add_transition("crouch_walk", "walk", "uncrouch", 0.2F);
    sm.add_transition("crouch", "crouch_walk", "start_moving", 0.15F);
    sm.add_transition("crouch_walk", "crouch", "stop_moving", 0.2F);

    sm.set_initial_state("idle");
}

void AnimationDriver::init_upper_body(StateMachine& sm) {
    sm.add_state("upper_idle", "anim_upper_idle");
    sm.add_state("upper_fire", "anim_upper_fire");
    sm.add_state("upper_reload", "anim_upper_reload");
    sm.add_state("upper_ads_idle", "anim_upper_ads_idle");
    sm.add_state("upper_ads_fire", "anim_upper_ads_fire");

    sm.add_transition("upper_idle", "upper_fire", "fire", 0.05F);
    sm.add_exit_time_transition("upper_fire", "upper_idle", 0.9F, 0.1F);

    sm.add_transition("upper_idle", "upper_reload", "reload", 0.1F);
    sm.add_exit_time_transition("upper_reload", "upper_idle", 0.95F, 0.15F);

    sm.add_transition("upper_idle", "upper_ads_idle", "ads", 0.2F);
    sm.add_transition("upper_ads_idle", "upper_idle", "unads", 0.2F);

    sm.add_transition("upper_ads_idle", "upper_ads_fire", "fire", 0.05F);
    sm.add_exit_time_transition("upper_ads_fire", "upper_ads_idle", 0.9F, 0.1F);

    sm.set_initial_state("upper_idle");
}

void AnimationDriver::set_weapon_config(const WeaponAnimConfig& config) {
    weapon_config_ = config;
}

void AnimationDriver::tick(const AnimGameplayInput& input, float dt,
                            StateMachine& sm,
                            AnimationGraph& graph,
                            WeaponAnimState& weapon_state,
                            skeleton::Mat4& out_weapon_transform,
                            std::vector<skeleton::Mat4>& out_pose) {
    // ── Locomotion triggers ────────────────────────────────────────

    // Movement state changes
    if (input.movement != previous_movement_) {
        switch (input.movement) {
            case AnimMovementState::Idle:
                sm.trigger("stop_moving");
                break;
            case AnimMovementState::Walking:
                sm.trigger("start_moving");
                break;
            case AnimMovementState::Sprinting:
                sm.trigger("start_sprinting");
                break;
            case AnimMovementState::Jumping:
                sm.trigger("jump");
                break;
            case AnimMovementState::Sliding:
                sm.trigger("slide");
                break;
            case AnimMovementState::Crouching:
                sm.trigger("crouch");
                break;
            default:
                break;
        }
    }

    // Ground state changes
    if (input.is_on_ground && !previous_on_ground_) {
        sm.trigger("landed");
    }

    // ── Blend parameter ────────────────────────────────────────────

    sm.set_blend_param(input.speed_normalized);

    // ── Tick state machine ─────────────────────────────────────────

    sm.tick(dt);

    // ── Evaluate animation graph ───────────────────────────────────

    graph.evaluate(sm.active_clips(), dt, out_pose);

    // ── Weapon animation (first-person) ────────────────────────────

    float player_speed = input.speed;
    bool is_moving = (input.movement == AnimMovementState::Walking ||
                      input.movement == AnimMovementState::Sprinting ||
                      input.movement == AnimMovementState::Crouching);

    evaluate_weapon_animation(weapon_state, weapon_config_,
                               dt, player_speed,
                               input.look_delta_x, input.look_delta_y,
                               is_moving, input.is_ads, input.is_firing,
                               out_weapon_transform);

    // ── Recoil ─────────────────────────────────────────────────────

    RecoilState& recoil = weapon_state.recoil;
    if (input.fire_pressed_this_frame) {
        fire_recoil(recoil, weapon_config_.recoil, input.is_ads);
    }

    JointTransform recoil_offset = JointTransform::identity();
    apply_recoil(recoil, weapon_config_.recoil, dt, input.is_firing, input.is_ads,
                 recoil_offset);

    // Apply recoil to weapon transform
    if (recoil_offset.qx != 0.0F || recoil_offset.qy != 0.0F ||
        recoil_offset.qz != 0.0F || recoil_offset.qw != 1.0F) {
        skeleton::Mat4 recoil_mat = recoil_offset.to_mat4();
        out_weapon_transform = recoil_mat * out_weapon_transform;
    }

    // ── Store previous state ──────────────────────────────────────

    previous_movement_ = input.movement;
    previous_on_ground_ = input.is_on_ground;
    previous_firing_ = input.is_firing;
}

void AnimationDriver::compute_aim_offset(const AnimGameplayInput& input,
                                          const AimOffsetConfig& config,
                                          std::vector<JointTransform>& out_additive) {
    // Compute relative yaw (difference between look direction and body facing)
    float relative_yaw = input.aim_yaw - input.body_yaw;
    float relative_pitch = input.aim_pitch;

    // Clamp to max offsets
    relative_yaw = std::clamp(relative_yaw, -config.max_spine_yaw, config.max_spine_yaw);
    relative_pitch = std::clamp(relative_pitch, -config.max_spine_pitch, config.max_spine_pitch);

    // Distribute across joints
    if (config.spine_joint >= 0 &&
        static_cast<std::size_t>(config.spine_joint) < out_additive.size()) {
        float spine_yaw = relative_yaw * config.spine_yaw_share;
        float spine_pitch = relative_pitch * config.spine_pitch_share;
        // Create Y rotation (yaw) then X rotation (pitch)
        float half_yaw = spine_yaw * 0.5F;
        float half_pitch = spine_pitch * 0.5F;
        float cy = std::cos(half_yaw), sy = std::sin(half_yaw);
        float cp = std::cos(half_pitch), sp = std::sin(half_pitch);

        auto& t = out_additive[static_cast<std::size_t>(config.spine_joint)];
        // Combine Y * X rotation
        t.qx = sp * cy;
        t.qy = sy * cp;
        t.qz = -sy * sp;
        t.qw = cp * cy;
    }

    if (config.neck_joint >= 0 &&
        static_cast<std::size_t>(config.neck_joint) < out_additive.size()) {
        float neck_yaw = relative_yaw * config.neck_yaw_share;
        float neck_pitch = relative_pitch * config.neck_pitch_share;
        float half_yaw = neck_yaw * 0.5F;
        float half_pitch = neck_pitch * 0.5F;
        float cy = std::cos(half_yaw), sy = std::sin(half_yaw);
        float cp = std::cos(half_pitch), sp = std::sin(half_pitch);

        auto& t = out_additive[static_cast<std::size_t>(config.neck_joint)];
        t.qx = sp * cy;
        t.qy = sy * cp;
        t.qz = -sy * sp;
        t.qw = cp * cy;
    }
}

}  // namespace ae::animation
