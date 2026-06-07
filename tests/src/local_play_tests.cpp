#include "ahamkara/client/local_play.h"
#include "ahamkara/game/net_types.h"
#include <cassert>
#include <iostream>
#include <memory>

namespace {

// A mock input provider that allows tests to control the inputs returned
class MockInputProvider : public ahamkara::client::IInputProvider {
public:
    ahamkara::game::PlayerInputCommand gather_input(float delta_seconds) override {
        (void)delta_seconds;
        return next_command;
    }

    ahamkara::game::PlayerInputCommand next_command {};
};

void test_local_play_simulation_tick_progression() {
    auto mock_input = std::make_unique<MockInputProvider>();
    auto* mock_ptr = mock_input.get();

    ahamkara::client::LocalPlaySimulation simulation(std::move(mock_input));
    simulation.set_colliders(nullptr, 0);

    assert(simulation.get_current_tick() == 0);
    assert(simulation.get_total_elapsed_seconds() == 0.0F);
    assert(simulation.get_last_delta_seconds() == 0.0F);

    // Setup input command
    mock_ptr->next_command.move_axis.y = 1.0F; // Move forward
    mock_ptr->next_command.sprint_held = true;

    // Simulate 1 tick
    simulation.tick(1.0F);

    assert(simulation.get_current_tick() == 1);
    assert(simulation.get_last_delta_seconds() == 1.0F);
    assert(simulation.get_total_elapsed_seconds() == 1.0F);

    const auto& player = simulation.get_player_state();
    std::cout << "DEBUG PROGRESSION: z_pos=" << player.position.z << " z_vel=" << player.velocity.z << " movement_state=" << (int)player.movement_state << "\n";
    // With the Quake/Source acceleration model, the player accelerates from 0
    // over the tick.  After 1.0s of sprint input the player should have moved
    // forward meaningfully and reached or approached max sprint speed (6.0 m/s).
    assert(player.position.z > 4.0F);   // moved substantially forward
    assert(std::fabs(player.velocity.z - 6.0F) < 0.01F);  // reached sprint speed
    assert(player.movement_state == ahamkara::game::MovementState::Sprinting);

    const auto& camera = simulation.get_camera_anchor();
    // Camera is attached to player eye height (+0.58m in the debug scale)
    // Head bob adds a small sinusoidal offset, so use a tolerance.
    assert(camera.position.z == player.position.z);
    assert(std::fabs(camera.position.y - (player.position.y + 0.58F)) < 0.1F);

    std::cout << "test_local_play_simulation_tick_progression passed.\n";
}

void test_local_play_simulation_timing() {
    auto mock_input = std::make_unique<MockInputProvider>();
    ahamkara::client::LocalPlaySimulation simulation(std::move(mock_input));

    simulation.tick(0.016F);
    assert(simulation.get_current_tick() == 1);
    assert(simulation.get_last_delta_seconds() == 0.016F);
    assert(simulation.get_total_elapsed_seconds() == 0.016F);

    simulation.tick(0.016F);
    assert(simulation.get_current_tick() == 2);
    assert(simulation.get_last_delta_seconds() == 0.016F);
    assert(simulation.get_total_elapsed_seconds() == 0.032F);

    std::cout << "test_local_play_simulation_timing passed.\n";
}

void test_local_play_simulation_input_swap() {
    // Tests that we can subclass IInputProvider with custom logic
    class CustomProvider : public ahamkara::client::IInputProvider {
    public:
        ahamkara::game::PlayerInputCommand gather_input(float delta_seconds) override {
            (void)delta_seconds;
            ahamkara::game::PlayerInputCommand cmd {};
            cmd.move_axis.x = 1.0F; // Constant strafe right
            return cmd;
        }
    };

    auto custom = std::make_unique<CustomProvider>();
    ahamkara::client::LocalPlaySimulation simulation(std::move(custom));
    simulation.set_colliders(nullptr, 0);

    simulation.tick(1.0F); // Walk speed is 3.0 m/s, yaw=0 → right = +X, start at X=-12
    // With acceleration model the player ramps up to walk speed; position will be
    // less than the old instant-velocity value (-9.0) but still positive movement.
    float final_x = simulation.get_player_state().position.x;
    assert(final_x > -9.5F);  // moved right from -12
    assert(final_x < -8.5F);  // but not all the way to -9 instantly
    assert(simulation.get_player_state().movement_state == ahamkara::game::MovementState::Walking);

    std::cout << "test_local_play_simulation_input_swap passed.\n";
}

} // namespace

void run_local_play_tests() {
    test_local_play_simulation_tick_progression();
    test_local_play_simulation_timing();
    test_local_play_simulation_input_swap();
}
