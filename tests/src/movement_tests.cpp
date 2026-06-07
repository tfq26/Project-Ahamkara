#include "ahamkara/game/movement.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

using namespace ahamkara::game;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

[[nodiscard]] float len_h(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.z * v.z);
}

// ---------------------------------------------------------------------------
// Acceleration model tests
// ---------------------------------------------------------------------------

/// Sprint forward from rest: after one tick velocity should grow but not
/// instantly hit max speed.
void test_acceleration_ramp_up() {
    ReplicatedPlayerState ps {};
    MovementSimState ss {};
    PlayerInputCommand cmd {};
    cmd.move_axis.y = 1.0F;   // full forward
    cmd.sprint_held = true;

    accelerate_movement(ps, ss, cmd, 0.016F);

    // With ground_accel=12, sprint_speed=6, dt=0.016:
    // accel_speed = min(12 * 0.016 * 6, 6 - 0) = min(1.152, 6) = 1.152
    // So vel.z ≈ 1.152
    assert(std::fabs(ps.velocity.z - 1.152F) < 0.01F);
    assert(ps.velocity.x == 0.0F);
    assert(ps.movement_state == MovementState::Sprinting);

    std::cout << "test_acceleration_ramp_up passed.\n";
}

/// After many ticks the speed should converge to max sprint speed.
void test_acceleration_reaches_max() {
    ReplicatedPlayerState ps {};
    MovementSimState ss {};
    PlayerInputCommand cmd {};
    cmd.move_axis.y = 1.0F;
    cmd.sprint_held = true;

    constexpr int kTicks = 60; // 60 * 0.016 = 0.96s
    for (int i = 0; i < kTicks; ++i) {
        accelerate_movement(ps, ss, cmd, 0.016F);
    }

    float h_speed = len_h(ps.velocity);
    // Should be very close to sprint_speed=6.0
    assert(h_speed > 5.8F && h_speed <= 6.0F);

    std::cout << "test_acceleration_reaches_max passed (speed=" << h_speed << ").\n";
}

/// When no input is given, ground friction should bring the player to a stop.
void test_friction_decelerates() {
    ReplicatedPlayerState ps {};
    MovementSimState ss {};
    ps.velocity.z = 6.0F;  // already at max speed
    ss.was_on_ground = true;

    PlayerInputCommand cmd {};  // no input

    // One tick of friction: drop = 8.0 * 0.016 * 6.0 = 0.768
    accelerate_movement(ps, ss, cmd, 0.016F);

    assert(ps.velocity.z < 6.0F);
    assert(ps.velocity.z > 5.0F);   // still substantial speed after one tick
    assert(ps.movement_state == MovementState::Idle);

    // After many ticks the player should stop
    for (int i = 0; i < 200; ++i) {
        accelerate_movement(ps, ss, cmd, 0.016F);
    }

    assert(len_h(ps.velocity) < 0.01F);

    std::cout << "test_friction_decelerates passed.\n";
}

/// Air acceleration should be much slower than ground acceleration.
void test_air_accel_slower_than_ground() {
    // Ground test
    {
        ReplicatedPlayerState ps {};
        MovementSimState ss {};
        PlayerInputCommand cmd {};
        cmd.move_axis.y = 1.0F;
        cmd.sprint_held = true;

        accelerate_movement(ps, ss, cmd, 0.016F);
        float ground_vel_z = ps.velocity.z;

        // ground_accel=12, dt=0.016, wishspeed=6
        // accel = min(12*0.016*6, 6) = 1.152
        assert(std::fabs(ground_vel_z - 1.152F) < 0.01F);
    }

    // Air test
    {
        ReplicatedPlayerState ps {};
        MovementSimState ss {};
        ss.was_on_ground = false;   // simulate being airborne
        ps.position.y = 1.0F;       // above ground
        PlayerInputCommand cmd {};
        cmd.move_axis.y = 1.0F;
        cmd.sprint_held = true;

        accelerate_movement(ps, ss, cmd, 0.016F);

        // air_accel=1.5, dt=0.016, wishspeed=6
        // accel = min(1.5*0.016*6, 6) = 0.144
        assert(std::fabs(ps.velocity.z - 0.144F) < 0.01F);
    }

    std::cout << "test_air_accel_slower_than_ground passed.\n";
}

// ---------------------------------------------------------------------------
// Jump buffering tests
// ---------------------------------------------------------------------------

/// Jump pressed while airborne is buffered and triggered on landing.
void test_jump_buffer_on_landing() {
    ReplicatedPlayerState ps {};
    MovementSimState ss {};
    ss.was_on_ground = false;
    ps.position.y = 1.0F;  // airborne
    ps.velocity.y = -2.0F; // falling

    // Tick 1: press jump while in air — gets buffered
    PlayerInputCommand cmd {};
    cmd.jump_pressed = true;
    accelerate_movement(ps, ss, cmd, 0.016F);

    // velocity.y should still be negative (falling, no jump yet)
    assert(ps.velocity.y < 0.0F);
    assert(ss.jump_buffer_timer > 0.0F);

    // Simulate falling to ground
    ps.position.y = 0.0F;
    ps.velocity.y = 0.0F;

    // Tick 2: landing with buffer still active — should trigger jump
    cmd.jump_pressed = false;   // not pressing jump anymore
    accelerate_movement(ps, ss, cmd, 0.016F);

    // Jump should have triggered: velocity.y = jump_speed = 5.5
    assert(std::fabs(ps.velocity.y - 5.5F) < 0.01F);
    assert(ss.jump_buffer_timer == 0.0F);  // buffer consumed

    std::cout << "test_jump_buffer_on_landing passed.\n";
}

/// Buffered jump expires after the buffer window.
void test_jump_buffer_expires() {
    ReplicatedPlayerState ps {};
    MovementSimState ss {};
    ss.was_on_ground = false;
    ps.position.y = 1.0F;
    ps.velocity.y = -2.0F;

    // Press jump in air
    PlayerInputCommand cmd {};
    cmd.jump_pressed = true;
    accelerate_movement(ps, ss, cmd, 0.016F);

    assert(ss.jump_buffer_timer > 0.0F);

    // Simulate time passing beyond buffer window (0.15s default)
    for (int i = 0; i < 20; ++i) {
        cmd.jump_pressed = false;
        ps.position.y = 1.0F;  // stay airborne
        ps.velocity.y = -2.0F;
        accelerate_movement(ps, ss, cmd, 0.016F);
    }

    // Buffer should have expired
    assert(ss.jump_buffer_timer == 0.0F);

    // Land
    ps.position.y = 0.0F;
    ps.velocity.y = 0.0F;

    // Tick after landing — buffer expired, no jump
    accelerate_movement(ps, ss, cmd, 0.016F);
    assert(ps.velocity.y <= 0.0F);  // no jump triggered

    std::cout << "test_jump_buffer_expires passed.\n";
}

// ---------------------------------------------------------------------------
// Coyote time tests
// ---------------------------------------------------------------------------

/// Coyote time allows jumping briefly after leaving ground.
void test_coyote_time_jump() {
    ReplicatedPlayerState ps {};
    MovementSimState ss {};
    ss.was_on_ground = true;   // was on ground last tick
    ps.position.y = 0.01F;     // barely above ground
    ps.velocity.y = -0.5F;

    // Tick 1: walk off ledge, press jump within coyote window
    PlayerInputCommand cmd {};
    cmd.jump_pressed = true;
    accelerate_movement(ps, ss, cmd, 0.016F);

    // Should have jumped despite not being on ground
    assert(std::fabs(ps.velocity.y - 5.5F) < 0.01F);
    assert(ss.coyote_timer == 0.0F);  // consumed

    std::cout << "test_coyote_time_jump passed.\n";
}

/// Coyote time expires and jump is no longer allowed while still airborne.
void test_coyote_time_expires() {
    ReplicatedPlayerState ps {};
    MovementSimState ss {};
    ss.was_on_ground = true;
    ps.position.y = 100.0F;  // start high so player stays airborne
    ps.velocity.y = -0.1F;

    // Simulate falling for longer than coyote window (but stay airborne)
    PlayerInputCommand cmd {};
    for (int i = 0; i < 12; ++i) {
        cmd.jump_pressed = false;
        accelerate_movement(ps, ss, cmd, 0.016F);
    }

    // Coyote should have expired (12 * 0.016 = 0.192s > 0.10s default)
    assert(ss.coyote_timer == 0.0F);
    assert(ps.position.y > 50.0F);  // still airborne

    // Now press jump — should NOT jump (coyote expired, still airborne)
    cmd.jump_pressed = true;
    float vy_before = ps.velocity.y;
    accelerate_movement(ps, ss, cmd, 0.016F);
    // Velocity should still be negative (falling), just increased by gravity
    assert(ps.velocity.y < vy_before);  // gravity still applied, falling faster

    std::cout << "test_coyote_time_expires passed.\n";
}

// ---------------------------------------------------------------------------
// Gravity tests
// ---------------------------------------------------------------------------

/// Gravity should accelerate the player downward when airborne.
void test_gravity_accelerates_downward() {
    // Short timestep so the player doesn't hit the ground mid-flight.
    ReplicatedPlayerState ps {};
    MovementSimState ss {};
    ss.was_on_ground = false;
    ps.position.y = 100.0F;  // high up so we don't hit ground

    accelerate_movement(ps, ss, {}, 0.5F);

    // After 0.5s: vy = 0 - 18.0 * 0.5 = -9.0
    assert(std::fabs(ps.velocity.y + 9.0F) < 0.1F);
    // Position: 100 + (-9.0) * 0.5 = 95.5
    assert(std::fabs(ps.position.y - 95.5F) < 0.1F);

    std::cout << "test_gravity_accelerates_downward passed.\n";
}

/// Player should be clamped to ground and not fall through.
void test_ground_clamp() {
    ReplicatedPlayerState ps {};
    MovementSimState ss {};
    ps.position.y = -5.0F;  // below ground

    accelerate_movement(ps, ss, {}, 0.016F);

    assert(ps.position.y == 0.0F);
    assert(ps.velocity.y == 0.0F);

    std::cout << "test_ground_clamp passed.\n";
}

// ---------------------------------------------------------------------------
// Resolution / backward-compat tests
// ---------------------------------------------------------------------------

/// resolve_movement_state should work as before.
void test_resolve_movement_state() {
    PlayerInputCommand cmd {};

    assert(resolve_movement_state(cmd) == MovementState::Idle);

    cmd.move_axis.y = 1.0F;
    assert(resolve_movement_state(cmd) == MovementState::Walking);

    cmd.sprint_held = true;
    assert(resolve_movement_state(cmd) == MovementState::Sprinting);

    cmd.jump_pressed = true;
    assert(resolve_movement_state(cmd) == MovementState::Jumping);

    cmd = {};
    cmd.slide_pressed = true;
    assert(resolve_movement_state(cmd) == MovementState::Sliding);

    std::cout << "test_resolve_movement_state passed.\n";
}

/// Original simulate_player_movement should still work.
void test_simulate_player_movement_backward_compat() {
    ReplicatedPlayerState ps {};
    PlayerInputCommand cmd {};
    cmd.move_axis.y = 1.0F;
    cmd.sprint_held = true;

    simulate_player_movement(ps, cmd, 1.0F);

    assert(std::fabs(ps.position.z - 8.0F) < 0.001F);
    assert(std::fabs(ps.velocity.z - 8.0F) < 0.001F);
    assert(ps.movement_state == MovementState::Sprinting);

    std::cout << "test_simulate_player_movement_backward_compat passed.\n";
}

// ---------------------------------------------------------------------------
// Config overrides
// ---------------------------------------------------------------------------

/// Custom MovementConfig should be respected.
void test_custom_config() {
    MovementConfig cfg {};
    cfg.walk_speed = 4.0F;
    cfg.ground_accel = 20.0F;
    cfg.gravity = 10.0F;
    cfg.jump_speed = 8.0F;

    ReplicatedPlayerState ps {};
    MovementSimState ss {};

    PlayerInputCommand cmd {};
    cmd.move_axis.y = 1.0F;
    cmd.jump_pressed = true;

    accelerate_movement(ps, ss, cmd, 0.016F, cfg);

    // Jump: vy = 8.0 (custom)
    assert(std::fabs(ps.velocity.y - 8.0F) < 0.01F);

    // Acceleration: accel_rate=20, wishspeed=4 (walk), dt=0.016
    // accel = min(20*0.016*4, 4) = min(1.28, 4) = 1.28
    assert(std::fabs(ps.velocity.z - 1.28F) < 0.01F);

    std::cout << "test_custom_config passed.\n";
}

/// is_on_ground_simple detection.
void test_is_on_ground_simple() {
    ReplicatedPlayerState ps {};

    ps.position.y = 0.0F;
    assert(is_on_ground_simple(ps));

    ps.position.y = 0.0005F;  // within epsilon
    assert(is_on_ground_simple(ps));

    ps.position.y = 0.01F;
    assert(!is_on_ground_simple(ps));

    // Custom ground height
    assert(is_on_ground_simple(ps, 1.0F));
    assert(!is_on_ground_simple(ps, -5.0F));

    std::cout << "test_is_on_ground_simple passed.\n";
}

}  // namespace

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

int main() {
    test_acceleration_ramp_up();
    test_acceleration_reaches_max();
    test_friction_decelerates();
    test_air_accel_slower_than_ground();
    test_jump_buffer_on_landing();
    test_jump_buffer_expires();
    test_coyote_time_jump();
    test_coyote_time_expires();
    test_gravity_accelerates_downward();
    test_ground_clamp();
    test_resolve_movement_state();
    test_simulate_player_movement_backward_compat();
    test_custom_config();
    test_is_on_ground_simple();

    std::cout << "\nAll movement tests passed.\n";
    return 0;
}
