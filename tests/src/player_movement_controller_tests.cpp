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
        3.0F,
        6.0F,
        5.5F,
        18.0F);

    assert(player.yaw == 10.0F);
    assert(controller.desired_velocity().z > 0.0F);

    controller.finish_frame(player, input, 0.016F, true, nullptr, 0, nullptr);

    assert(player.movement_state == MovementState::Sprinting);
    assert(controller.camera_anchor().pitch >= -89.0F);
    assert(controller.movement_debug().on_ground == true);
}

}  // namespace

int main() {
    test_controller_reset_and_camera();
    test_controller_updates_locomotion_state();
    std::cout << "player_movement_controller tests passed.\n";
    return 0;
}
