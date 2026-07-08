#include "world_camera.h"

#include "ae/core/math.h"

#include <algorithm>
#include <cmath>

namespace ahamkara::game {

// --- update_camera_state ------------------------------------------------------

void update_camera_state(
    CameraAnchor& anchor,
    const ReplicatedPlayerState& player_state,
    MovementSimState& sim_state,
    float delta_seconds,
    const PlayerInputCommand& input,
    bool crouch_active) {

    // Apply look delta (mouse input)
    anchor.yaw += input.look_delta.x;
    anchor.pitch += input.look_delta.y;

    // Clamp pitch to [-89, 89] degrees
    anchor.pitch = std::max(-89.0F, std::min(89.0F, anchor.pitch));

    // Wrap yaw to [-180, 180]
    anchor.yaw = std::fmod(anchor.yaw + 180.0F, 360.0F);
    if (anchor.yaw < 0.0F) anchor.yaw += 360.0F;
    anchor.yaw -= 180.0F;

    // Position camera at player position with eye height offset
    float eye_height = crouch_active ? 0.32F : 0.58F;
    anchor.position.x = player_state.position.x;
    anchor.position.y = player_state.position.y + eye_height;
    anchor.position.z = player_state.position.z;

    // Head bob (only when on ground and moving)
    bool has_input = has_move_input(input);
    float h_speed = std::sqrt(
        player_state.velocity.x * player_state.velocity.x +
        player_state.velocity.z * player_state.velocity.z);

    if (has_input && h_speed > 0.1F) {
        // Simple sine-based head bob
        constexpr float kBobFreq = 10.0F;
        constexpr float kBobAmp = 0.015F;
        sim_state.head_bob_phase += delta_seconds * kBobFreq * std::min(h_speed / 6.0F, 1.0F);
        float bob = std::sin(sim_state.head_bob_phase) * kBobAmp;
        anchor.position.y += bob;
        // Slight horizontal sway
        float sway = std::cos(sim_state.head_bob_phase * 0.5F) * kBobAmp * 0.5F;
        anchor.position.x += sway * std::sin(ae::to_radians(anchor.yaw + 90.0F));
        anchor.position.z += sway * std::cos(ae::to_radians(anchor.yaw + 90.0F));
    } else {
        // Decay bob phase toward 0
        sim_state.head_bob_phase *= 0.9F;
    }
}

// --- resolve_movement_state ---------------------------------------------------

void resolve_movement_state(
    ReplicatedPlayerState& player_state,
    float slide_timer_seconds,
    const MovementSimState& sim_state,
    const PlayerInputCommand& input,
    bool on_ground) {

    bool has_input = has_move_input(input);

    if (sim_state.ledge_grabbed) {
        player_state.movement_state = MovementState::LedgeGrab;
    } else if (sim_state.is_mantling) {
        player_state.movement_state = MovementState::Mantling;
    } else if (sim_state.on_ladder) {
        player_state.movement_state = MovementState::OnLadder;
    } else if (!on_ground || player_state.velocity.y > 0.001F) {
        player_state.movement_state = MovementState::Jumping;
    } else if (slide_timer_seconds > 0.0F && has_input) {
        player_state.movement_state = MovementState::Sliding;
    } else if (input.sprint_held && has_input) {
        player_state.movement_state = MovementState::Sprinting;
    } else if (has_input) {
        player_state.movement_state = MovementState::Walking;
    } else {
        player_state.movement_state = MovementState::Idle;
    }
}

// --- fill_movement_debug ------------------------------------------------------

void fill_movement_debug(
    MovementDebugState& debug,
    const ReplicatedPlayerState& player_state,
    const MovementSimState& sim_state,
    float slide_timer_seconds,
    float delta_seconds,
    const PlayerInputCommand& input,
    bool on_ground) {

    (void)delta_seconds;
    (void)input;

    debug.velocity_vector = player_state.velocity;
    debug.ground_normal = sim_state.ground_normal;
    debug.ground_material = sim_state.ground_material;

    // Jump buffer: 0..1 pct
    float max_buffer = 0.15F;
    debug.jump_buffer_pct = sim_state.jump_buffer_timer > 0.0F
        ? sim_state.jump_buffer_timer / max_buffer : 0.0F;

    // Coyote timer: 0..1 pct
    float max_coyote = 0.10F;
    debug.coyote_pct = sim_state.coyote_timer > 0.0F
        ? sim_state.coyote_timer / max_coyote : 0.0F;

    // Slide: 0..1 pct of max slide duration (0.45F)
    debug.slide_pct = slide_timer_seconds > 0.0F
        ? slide_timer_seconds / 0.45F : 0.0F;

    debug.on_ground = on_ground;
    debug.on_ladder = sim_state.on_ladder;

    // Wish direction from camera yaw and input axis
    float yaw_rad = ae::to_radians(player_state.yaw);
    float wish_x = input.move_axis.x * std::cos(yaw_rad) + input.move_axis.y * std::sin(yaw_rad);
    float wish_z = -input.move_axis.x * std::sin(yaw_rad) + input.move_axis.y * std::cos(yaw_rad);
    debug.wish_direction = {wish_x, 0.0F, wish_z};
}

}  // namespace ahamkara::game
