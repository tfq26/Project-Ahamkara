#include "ahamkara/game/movement.h"

namespace ahamkara::game {

MovementState resolve_movement_state(const PlayerInputCommand& command) {
    if (command.jump_pressed) {
        return MovementState::Jumping;
    }

    if (command.slide_pressed) {
        return MovementState::Sliding;
    }

    if (command.sprint_held) {
        return MovementState::Sprinting;
    }

    if (command.move_axis.x != 0.0F || command.move_axis.y != 0.0F) {
        return MovementState::Walking;
    }

    return MovementState::Idle;
}

void simulate_player_movement(
    ReplicatedPlayerState& player_state,
    const PlayerInputCommand& command,
    float delta_seconds) {
    const float speed = command.sprint_held ? 8.0F : 5.0F;

    player_state.velocity = {
        command.move_axis.x * speed,
        0.0F,
        command.move_axis.y * speed
    };
    player_state.position.x += player_state.velocity.x * delta_seconds;
    player_state.position.z += player_state.velocity.z * delta_seconds;
    player_state.movement_state = resolve_movement_state(command);
}

}  // namespace ahamkara::game
