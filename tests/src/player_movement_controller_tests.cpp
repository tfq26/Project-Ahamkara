#include "ahamkara/game/player_movement_controller.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

using namespace ahamkara::game;

void test_controller_reset_and_camera() {
    PlayerMovementController controller;
    PlayerSpawnDefinition spawn {};
    spawn.position = {1.0F, 2.0F, 3.0F};
    spawn.yaw = 45.0F;
    controller.reset_to_spawn(spawn);

    assert(std::fabs(controller.camera_anchor().position.x - 1.0F) < 0.001F);
    assert(std::fabs(controller.camera_anchor().position.y - 2.0F) < 0.001F);
    assert(std::fabs(controller.camera_anchor().position.z - 3.0F) < 0.001F);
    assert(std::fabs(controller.camera_anchor().yaw - 45.0F) < 0.001F);
    assert(controller.crouch_active() == false);
}

void test_controller_updates_locomotion_state() {
    PlayerMovementController controller;
    controller.reset_to_spawn(PlayerSpawnDefinition {});

    ReplicatedPlayerState player {};
    player.yaw = 0.0F;

    PlayerInputCommand input {};
    input.move_axis.y = 1.0F;
    input.sprint_held = true;
    input.look_delta.x = 10.0F;
    input.look_delta.y = -5.0F;

    controller.begin_frame(
        player,
        input,
        0.016F,
        true,
        {0.0F, 0.0F, 0.0F},
        MovementConfig {});

    assert(player.yaw == 10.0F);
    assert(controller.desired_velocity().z > 0.0F);

    controller.finish_frame(player, input, 0.016F, true, nullptr, 0, nullptr);

    assert(player.movement_state == MovementState::Sprinting);
    assert(controller.camera_anchor().pitch >= -89.0F);
    assert(controller.movement_debug().on_ground == true);
}

void test_controller_respects_custom_movement_config() {
    PlayerMovementController controller;
    controller.reset_to_spawn(PlayerSpawnDefinition {});

    ReplicatedPlayerState player {};
    PlayerInputCommand input {};
    input.move_axis.y = 1.0F;
    input.slide_pressed = true;

    MovementConfig config {};
    config.slide_speed = 13.0F;
    config.slide_duration = 0.25F;
    config.slide_cooldown = 1.0F;
    config.ground_accel = 100.0F;
    config.head_bob_amplitude = 0.0F;

    controller.begin_frame(player, input, 0.016F, true, {}, config);
    assert(std::fabs(controller.desired_velocity().z - config.slide_speed) < 0.001F);
    controller.finish_frame(player, input, 0.016F, true, nullptr, 0, nullptr);
    assert(controller.movement_debug().slide_pct > 0.9F);
    assert(std::fabs(controller.camera_anchor().position.y - 0.32F) < 0.001F);
}

}  // namespace

int main() {
    test_controller_reset_and_camera();
    test_controller_updates_locomotion_state();
    test_controller_respects_custom_movement_config();
    std::cout << "player_movement_controller tests passed.\n";
    return 0;
}
