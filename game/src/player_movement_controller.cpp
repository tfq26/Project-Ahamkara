#include "ahamkara/game/player_movement_controller.h"

#include "game_physics.h"
#include "world_camera.h"

#include "ae/core/math.h"

#include <algorithm>
#include <cmath>

#include <Jolt/Physics/Character/CharacterVirtual.h>

namespace ahamkara::game {
namespace {

constexpr float kStandingEyeHeight = 0.58F;
constexpr float kCrouchingEyeHeight = 0.32F;
constexpr float kSlideDurationSeconds = 0.45F;

constexpr float kGroundAccel = 12.0F;
constexpr float kAirAccel = 1.5F;
constexpr float kGroundFriction = 8.0F;
constexpr float kAirFriction = 0.5F;
constexpr float kMaxAirSpeed = 30.0F;
constexpr float kJumpBufferTime = 0.15F;
constexpr float kCoyoteTime = 0.10F;
constexpr float kSlideSpeed = 10.0F;

}  // namespace

void PlayerMovementController::reset_to_spawn(const PlayerSpawnDefinition& spawn) {
    camera_anchor_.position = spawn.position;
    camera_anchor_.yaw = spawn.yaw;
    camera_anchor_.pitch = 0.0F;
    movement_sim_state_ = {};
    movement_debug_ = {};
    desired_velocity_ = {};
    slide_timer_seconds_ = 0.0F;
    crouch_active_ = false;
    on_ground_before_step_ = false;
    previous_vertical_velocity_ = 0.0F;
}

void PlayerMovementController::begin_frame(
    ReplicatedPlayerState& player_state,
    const PlayerInputCommand& input,
    float delta_seconds,
    bool on_ground,
    const Vec3& current_velocity,
    float walk_speed,
    float sprint_speed,
    float jump_speed,
    float gravity) {

    previous_vertical_velocity_ = current_velocity.y;
    on_ground_before_step_ = on_ground;

    slide_timer_seconds_ = std::max(0.0F, slide_timer_seconds_ - delta_seconds);
    if (input.slide_pressed && on_ground && has_move_input(input)) {
        slide_timer_seconds_ = kSlideDurationSeconds;
    }

    crouch_active_ = input.crouch_held || slide_timer_seconds_ > 0.0F;

    const float yaw_rad = ae::to_radians(player_state.yaw);
    const float forward_x = std::sin(yaw_rad);
    const float forward_z = std::cos(yaw_rad);
    const float right_x = std::cos(yaw_rad);
    const float right_z = -std::sin(yaw_rad);

    const float input_magnitude = std::min(
        std::sqrt(input.move_axis.x * input.move_axis.x + input.move_axis.y * input.move_axis.y),
        1.0F);
    float move_speed = walk_speed;
    if (slide_timer_seconds_ > 0.0F) {
        move_speed = kSlideSpeed;
    } else if (input.sprint_held) {
        move_speed = sprint_speed;
    }
    const float wish_speed = move_speed * input_magnitude;

    if (input.jump_pressed) {
        movement_sim_state_.jump_buffer_timer = kJumpBufferTime;
    } else if (movement_sim_state_.jump_buffer_timer > 0.0F) {
        movement_sim_state_.jump_buffer_timer =
            std::max(0.0F, movement_sim_state_.jump_buffer_timer - delta_seconds);
    }

    if (!on_ground && movement_sim_state_.was_on_ground) {
        movement_sim_state_.coyote_timer = kCoyoteTime;
    } else if (movement_sim_state_.coyote_timer > 0.0F) {
        movement_sim_state_.coyote_timer =
            std::max(0.0F, movement_sim_state_.coyote_timer - delta_seconds);
    }

    const bool can_jump = on_ground || (movement_sim_state_.coyote_timer > 0.0F);
    const bool want_jump = input.jump_pressed || (movement_sim_state_.jump_buffer_timer > 0.0F);

    float desired_vx = current_velocity.x;
    float desired_vy = current_velocity.y;
    float desired_vz = current_velocity.z;

    if (want_jump && can_jump && slide_timer_seconds_ <= 0.0F) {
        desired_vy = jump_speed;
        movement_sim_state_.jump_buffer_timer = 0.0F;
        movement_sim_state_.coyote_timer = 0.0F;
    } else if (!on_ground) {
        desired_vy -= gravity * delta_seconds;
    } else {
        desired_vy = 0.0F;
    }

    float surf_speed_mult = surface_speed_multiplier(movement_sim_state_.ground_material, MovementConfig{});
    float surf_fric_mult = surface_friction_multiplier(movement_sim_state_.ground_material, MovementConfig{});

    if (input_magnitude > 0.001F && slide_timer_seconds_ <= 0.0F) {
        const float inv_mag = 1.0F / input_magnitude;
        const float wish_x = input.move_axis.x * inv_mag;
        const float wish_y = input.move_axis.y * inv_mag;
        const float wish_dir_x = wish_x * right_x + wish_y * forward_x;
        const float wish_dir_z = wish_x * right_z + wish_y * forward_z;

        const float current_speed = desired_vx * wish_dir_x + desired_vz * wish_dir_z;
        const float add_speed = wish_speed * surf_speed_mult - current_speed;
        if (add_speed > 0.0F) {
            float accel_rate = on_ground ? kGroundAccel : kAirAccel;
            if (input.sprint_held && on_ground) accel_rate *= 1.0F;
            float accel_speed = accel_rate * delta_seconds * wish_speed * surf_speed_mult;
            if (accel_speed > add_speed) {
                accel_speed = add_speed;
            }
            desired_vx += accel_speed * wish_dir_x;
            desired_vz += accel_speed * wish_dir_z;
        }
    } else if (on_ground) {
        float h_speed = std::sqrt(desired_vx * desired_vx + desired_vz * desired_vz);
        if (h_speed > 0.001F) {
            float drop = kGroundFriction * surf_fric_mult * delta_seconds * h_speed;
            if (drop > h_speed) drop = h_speed;
            float scale = (h_speed - drop) / h_speed;
            desired_vx *= scale;
            desired_vz *= scale;
        }
    } else {
        float h_speed = std::sqrt(desired_vx * desired_vx + desired_vz * desired_vz);
        if (h_speed > 0.001F) {
            float drop = kAirFriction * delta_seconds * h_speed;
            if (drop > h_speed) drop = h_speed;
            float scale = (h_speed - drop) / h_speed;
            desired_vx *= scale;
            desired_vz *= scale;
        }
    }

    if (!on_ground) {
        float total_speed = std::sqrt(desired_vx * desired_vx + desired_vy * desired_vy + desired_vz * desired_vz);
        if (total_speed > kMaxAirSpeed) {
            float scale = kMaxAirSpeed / total_speed;
            desired_vx *= scale;
            desired_vy *= scale;
            desired_vz *= scale;
        }
    }

    player_state.yaw += input.look_delta.x;
    movement_sim_state_.was_on_ground = on_ground;
    desired_velocity_ = {desired_vx, desired_vy, desired_vz};
}

void PlayerMovementController::finish_frame(
    ReplicatedPlayerState& player_state,
    const PlayerInputCommand& input,
    float delta_seconds,
    bool on_ground,
    const ColliderBox* colliders,
    std::size_t collider_count,
    JPH::CharacterVirtual* character) {

    if (character) {
        resolve_mantle(player_state, colliders, collider_count, character);
        resolve_ladder_and_ledge(player_state, input, character);
    }

    update_camera_and_debug(player_state, input, delta_seconds, on_ground);

    if (on_ground && !on_ground_before_step_ && previous_vertical_velocity_ < -0.01F) {
        Vec3 landing = compute_landing_impulse(std::abs(previous_vertical_velocity_), MovementConfig{});
        camera_anchor_.position.x += landing.x;
        camera_anchor_.position.y += landing.y;
    }
}

void PlayerMovementController::resolve_mantle(
    ReplicatedPlayerState& player_state,
    const ColliderBox* colliders,
    std::size_t collider_count,
    JPH::CharacterVirtual* character) {

    if (player_state.velocity.y <= 0.5F) return;
    if (!colliders || collider_count == 0) return;

    const float feet_y = player_state.position.y;
    const float eye_y = feet_y + (crouch_active_ ? kCrouchingEyeHeight : kStandingEyeHeight);
    const float px = player_state.position.x;
    const float pz = player_state.position.z;
    constexpr float mantle_margin = 0.3F;

    for (std::size_t i = 0; i < collider_count; ++i) {
        const auto& c = colliders[i];
        if (c.wall || c.jump_through || !c.auto_step) continue;

        const bool in_x = px >= c.min_x - mantle_margin && px <= c.max_x + mantle_margin;
        const bool in_z = pz >= c.min_z - mantle_margin && pz <= c.max_z + mantle_margin;
        if (!in_x || !in_z) continue;

        if (eye_y < c.top_y) continue;

        const float dist_below = c.top_y - feet_y;
        if (dist_below < 0.2F || dist_below > 1.3F) continue;

        player_state.position.y = c.top_y;
        player_state.velocity.y = 0.0F;

        if (character) {
            character->SetPosition(JPH::RVec3(player_state.position.x, player_state.position.y, player_state.position.z));
            character->SetLinearVelocity(JPH::Vec3(player_state.velocity.x, player_state.velocity.y, player_state.velocity.z));
        }
        return;
    }
}

void PlayerMovementController::resolve_ladder_and_ledge(
    ReplicatedPlayerState& player_state,
    const PlayerInputCommand& input,
    JPH::CharacterVirtual* character) {

    if (movement_sim_state_.ground_material == SurfaceMaterial::Ladder) {
        movement_sim_state_.on_ladder = true;
    } else if (movement_sim_state_.on_ladder && player_state.position.y <= 0.05F) {
        movement_sim_state_.on_ladder = false;
    }
    if (movement_sim_state_.on_ladder && character) {
        character->SetLinearVelocity(JPH::Vec3(
            character->GetLinearVelocity().GetX(),
            input.move_axis.y * 4.0F,
            character->GetLinearVelocity().GetZ()));
    }
}

void PlayerMovementController::update_camera_and_debug(
    ReplicatedPlayerState& player_state,
    const PlayerInputCommand& input,
    float delta_seconds,
    bool on_ground) {

    update_camera_state(camera_anchor_, player_state, movement_sim_state_, delta_seconds, input, crouch_active_);
    resolve_movement_state(player_state, slide_timer_seconds_, movement_sim_state_, input, on_ground);
    fill_movement_debug(movement_debug_, player_state, movement_sim_state_, slide_timer_seconds_, delta_seconds, input, on_ground);
}

}  // namespace ahamkara::game
