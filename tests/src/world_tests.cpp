#include "ahamkara/game/world.h"
#include "ahamkara/game/net_types.h"

#include <cassert>
#include <iostream>

namespace {

void test_world_initialization() {
    ahamkara::game::World world;
    
    const auto& player = world.get_player_state();
    assert(player.position.x == -12.0F);
    assert(player.position.y == 0.0F);
    assert(player.position.z == 0.0F);
    
    const auto& camera = world.get_camera_anchor();
    // Camera should be at spawn initially
    assert(camera.position.x == -12.0F);
    assert(camera.position.y == 0.0F);
    assert(camera.position.z == 0.0F);
    
    std::cout << "test_world_initialization passed.\n";
}

void test_world_tick_movement() {
    ahamkara::game::World world;
    ahamkara::game::PlayerInputCommand input {};
    input.move_axis.y = 1.0F; // Move forward
    
    world.tick(1.0F, input);
    
    const auto& player = world.get_player_state();
    assert(player.position.z > 0.0F);
    
    const auto& camera = world.get_camera_anchor();
    assert(camera.position.z == player.position.z);
    assert(camera.position.y == player.position.y + 0.58F);
    
    std::cout << "test_world_tick_movement passed.\n";
}

void test_world_tick_rotation() {
    ahamkara::game::World world;
    ahamkara::game::PlayerInputCommand input {};
    input.look_delta.x = 10.0F;
    input.look_delta.y = -5.0F;
    
    world.tick(0.016F, input);
    
    const auto& player = world.get_player_state();
    assert(player.yaw == 10.0F);
    
    const auto& camera = world.get_camera_anchor();
    assert(camera.yaw == 10.0F);
    assert(camera.pitch == -5.0F);
    
    std::cout << "test_world_tick_rotation passed.\n";
}

void test_world_camera_yaw_wraps() {
    ahamkara::game::World world;

    // Accumulate yaw past 180 degrees and verify wrapping in CameraAnchor
    ahamkara::game::PlayerInputCommand input {};
    input.look_delta.x = 200.0F;
    world.tick(0.016F, input);

    const auto& camera = world.get_camera_anchor();
    // 200 degrees wraps to -160
    assert(camera.yaw <= -159.9F && camera.yaw >= -160.1F);
    // Player yaw is still raw accumulated (200), only camera anchor wraps
    assert(world.get_player_state().yaw == 200.0F);

    std::cout << "test_world_camera_yaw_wraps passed.\n";
}

void test_world_camera_pitch_clamped() {
    ahamkara::game::World world;

    ahamkara::game::PlayerInputCommand input {};
    input.look_delta.y = -100.0F; // Try to look too far down
    world.tick(0.016F, input);

    const auto& camera = world.get_camera_anchor();
    assert(camera.pitch >= -89.0F && camera.pitch <= -88.9F);

    // Reset and test looking too far up
    ahamkara::game::World world2;
    ahamkara::game::PlayerInputCommand input2 {};
    input2.look_delta.y = 100.0F;
    world2.tick(0.016F, input2);

    const auto& camera2 = world2.get_camera_anchor();
    assert(camera2.pitch >= 88.9F && camera2.pitch <= 89.0F);

    std::cout << "test_world_camera_pitch_clamped passed.\n";
}

void test_world_platform_standing() {
    ahamkara::game::World world;

    // Place player above the central platform and let gravity land them
    ahamkara::game::ReplicatedPlayerState init_state {};
    init_state.position.x = 0.0F;
    init_state.position.z = 0.0F;
    init_state.position.y = 2.0F; // Above the platform
    init_state.velocity.y = -1.0F; // Falling
    world.set_player_state(init_state);

    ahamkara::game::PlayerInputCommand input {};
    input.move_axis.y = 1.0F;
    world.tick(0.1F, input);

    // Player should now be standing on top of the central platform
    const auto& ps = world.get_player_state();
    assert(std::fabs(ps.position.y - 1.5F) <= 0.001F);
    assert(ps.velocity.y == 0.0F);
    assert(ps.movement_state == ahamkara::game::MovementState::Walking);

    std::cout << "test_world_platform_standing passed.\n";
}

void test_world_platform_walking_off() {
    ahamkara::game::World world;

    ahamkara::game::ReplicatedPlayerState init_state {};
    init_state.position.x = 0.0F;
    init_state.position.z = 0.0F;
    init_state.position.y = 1.5F; // On top of central platform
    init_state.velocity.y = 0.0F;
    world.set_player_state(init_state);

    ahamkara::game::PlayerInputCommand input {};
    input.move_axis.x = 1.0F;
    world.tick(2.0F, input); // Walk speed 3.0, so moves 6.0 units east — off the platform

    // Player should now be on the ground (fell off platform)
    const auto& ps = world.get_player_state();
    assert(ps.position.y == 0.0F);
    assert(ps.velocity.y == 0.0F);

    std::cout << "test_world_platform_walking_off passed.\n";
}

void test_world_wall_collision() {
    ahamkara::game::World world;

    // Place player inside the central pillar (wall): x=[-0.8,0.8], z=[-0.8,0.8]
    ahamkara::game::ReplicatedPlayerState init_state {};
    init_state.position.x = 0.0F;
    init_state.position.z = 0.0F;  // Inside the central pillar
    init_state.position.y = 1.0F;  // Within pillar's vertical range (0 to 3.5)
    init_state.velocity.y = 0.0F;
    world.set_player_state(init_state);

    // Tick with no movement input — horizontal resolution should push player out
    ahamkara::game::PlayerInputCommand input {};
    world.tick(0.016F, input);

    const auto& ps = world.get_player_state();
    // Player should have been pushed out of the pillar interior.
    // dx_min = 0 - (-0.8) = 0.8 is the first minimum → pushed to x = -0.8
    assert(ps.position.x <= -0.7F);
    // Ensure player is NOT still inside the pillar at original position
    assert(std::fabs(ps.position.x) > 0.1F || std::fabs(ps.position.z) > 0.1F);

    std::cout << "test_world_wall_collision passed.\n";
}

void test_world_jump_through() {
    ahamkara::game::World world;

    // Test 1: Jump up from below a jump-through platform — should pass through
    {
        ahamkara::game::ReplicatedPlayerState init_state {};
        init_state.position.x = 6.0F;   // Center of NE bridge (X range [5,9])
        init_state.position.z = 6.0F;   // Center of NE bridge (Z range [5,9])
        init_state.position.y = 0.5F;   // Below the bridge (top_y = 1.15)
        init_state.velocity.y = 8.0F;   // Moving upward fast enough
        world.set_player_state(init_state);

        ahamkara::game::PlayerInputCommand input {};
        world.tick(0.3F, input);

        const auto& ps = world.get_player_state();
        // Player should pass through: y = 0.5 + 8.0*0.15 = 1.7, above bridge top at 1.15
        // Gravity: vy = 8.0 - 18.0*0.3 = 2.6, still moving up (not falling)
        assert(ps.position.y > 1.15F);
        assert(ps.velocity.y > 0.0F);
    }

    // Test 2: Fall onto a jump-through platform from above — should land
    {
        ahamkara::game::World world2;
        ahamkara::game::ReplicatedPlayerState init_state {};
        init_state.position.x = 6.0F;
        init_state.position.z = 6.0F;
        init_state.position.y = 1.5F;    // Above the bridge
        init_state.velocity.y = -1.0F;   // Falling
        world2.set_player_state(init_state);

        ahamkara::game::PlayerInputCommand input {};
        world2.tick(0.1F, input);

        const auto& ps = world2.get_player_state();
        // After gravity: y = 1.5 - 1.0*0.1 = 1.4, vy = -2.8
        // feet_y=1.4 > top_y=1.15 and falling → snap to 1.15
        assert(std::fabs(ps.position.y - 1.15F) <= 0.001F);
        assert(ps.velocity.y == 0.0F);
    }

    std::cout << "test_world_jump_through passed.\n";
}

} // namespace

int main() {
    test_world_initialization();
    test_world_tick_movement();
    test_world_tick_rotation();
    test_world_camera_yaw_wraps();
    test_world_camera_pitch_clamped();
    test_world_platform_standing();
    test_world_platform_walking_off();
    test_world_wall_collision();
    test_world_jump_through();
    
    std::cout << "All world tests passed!\n";
    return 0;
}
