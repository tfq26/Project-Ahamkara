#pragma once

#include "ahamkara/game/net_types.h"

#include <cstdint>

namespace ahamkara::game {

// ---------------------------------------------------------------------------
// Surface material — tagged on colliders, affects movement multipliers.
// ---------------------------------------------------------------------------
enum class SurfaceMaterial : std::uint8_t {
    Default = 0,
    Concrete,
    Metal,
    Wood,
    Dirt,
    Grass,
    Ice,       // low friction
    Mud,       // high friction, speed penalty
    Sand,      // moderate speed penalty
    Ladder,    // triggers ladder state
    Count
};

// ---------------------------------------------------------------------------
// Movement configuration — all tunable constants for the acceleration model.
// Use Quake / Source defaults unless overridden.
// ---------------------------------------------------------------------------
struct MovementConfig {
    // -- Speeds ------------------------------------------------------------
    float walk_speed          = 3.0F;
    float sprint_speed        = 6.0F;
    float slide_speed         = 10.0F;
    float ladder_speed        = 4.0F;
    float crouch_speed        = 1.5F;

    // -- Acceleration ------------------------------------------------------
    float ground_accel        = 12.0F;
    float air_accel           = 1.5F;
    float ladder_accel        = 20.0F;

    // -- Friction ----------------------------------------------------------
    float ground_friction     = 8.0F;
    float air_friction        = 0.5F;

    // -- Jump / Gravity ----------------------------------------------------
    float gravity             = 18.0F;
    float jump_speed          = 5.5F;
    float jump_buffer_time    = 0.15F;
    float coyote_time         = 0.10F;
    float max_air_speed       = 30.0F;

    // -- Slope / Step (area 32) --------------------------------------------
    float max_walkable_slope  = 45.0F;   // degrees — slide above this
    float slope_slide_accel   = 6.0F;    // extra downhill acceleration
    float step_height         = 0.4F;    // max auto-step height (m)

    // -- Slide (area 35) ---------------------------------------------------
    float slide_duration      = 0.45F;   // seconds
    float slide_cooldown      = 0.8F;    // seconds before next slide

    // -- Sprint (area 35) --------------------------------------------------
    float sprint_accel_mult   = 1.0F;    // multiplier on ground_accel when sprinting
    float sprint_turn_penalty = 0.85F;   // speed retention when sharp-turning

    // -- Ladder / Ledge (area 36) ------------------------------------------
    float ladder_dismount_speed = 3.0F;  // upward velocity on dismount
    float ledge_grab_range      = 0.6F;  // max horizontal reach to grab a ledge
    float mantle_up_speed       = 3.0F;  // upward speed during mantle animation

    // -- Head bob (area 38) ------------------------------------------------
    float head_bob_amplitude   = 0.025F; // meters vertical
    float head_bob_frequency   = 8.0F;   // cycles per second at max speed
    float head_bob_roll_amount = 0.015F; // radians
    float landing_impulse_mag  = 0.06F;  // meters downward jolt on landing

    // -- Ground fallback ---------------------------------------------------
    float ground_y            = 0.0F;

    // -- Per-surface multipliers (area 40) ---------------------------------
    /// Index by SurfaceMaterial enum value.
    float surface_speed_mult[static_cast<int>(SurfaceMaterial::Count)] = {
        1.0F,   // Default
        1.0F,   // Concrete
        0.95F,  // Metal
        0.9F,   // Wood
        0.85F,  // Dirt
        0.9F,   // Grass
        0.3F,   // Ice   — very slippery (low friction)
        0.5F,   // Mud   — slow
        0.7F,   // Sand  — moderately slow
        1.0F    // Ladder
    };

    float surface_friction_mult[static_cast<int>(SurfaceMaterial::Count)] = {
        1.0F,   // Default
        1.0F,   // Concrete
        0.9F,   // Metal
        0.8F,   // Wood
        1.2F,   // Dirt
        1.0F,   // Grass
        0.05F,  // Ice   — nearly frictionless
        2.0F,   // Mud   — sticky
        1.3F,   // Sand
        1.0F    // Ladder
    };
};

// ---------------------------------------------------------------------------
// Per-entity simulation state that persists across ticks.
// ---------------------------------------------------------------------------
struct MovementSimState {
    // -- Jump / coyote timers ----------------------------------------------
    float jump_buffer_timer  = 0.0F;
    float coyote_timer       = 0.0F;
    bool  was_on_ground      = false;

    // -- Slide (area 35) ---------------------------------------------------
    float slide_cooldown_timer = 0.0F;

    // -- Ladder / Ledge (area 36) ------------------------------------------
    bool  on_ladder          = false;
    Vec3  ladder_top {};          // world position of ladder top exit
    Vec3  ladder_bottom {};       // world position of ladder bottom exit
    Vec3  ladder_axis {};         // normalized climb direction
    bool  ledge_grabbed       = false;
    Vec3  ledge_point {};         // point where ledge was grabbed
    bool  is_mantling         = false;
    float mantle_timer        = 0.0F;

    // -- Moving platform (area 37) -----------------------------------------
    bool  on_moving_platform  = false;
    Vec3  platform_last_pos {};   // for delta tracking
    std::uint32_t platform_body_id = 0;

    // -- Head bob phase (area 38) -----------------------------------------
    float head_bob_phase      = 0.0F;

    // -- Ground contact info (area 33) ------------------------------------
    Vec3  ground_normal       = {0.0F, 1.0F, 0.0F}; // world-space up
    SurfaceMaterial ground_material = SurfaceMaterial::Default;
};

// ---------------------------------------------------------------------------
// Debug visualization state (area 39) — populated each tick for debug renderer
// ---------------------------------------------------------------------------
struct MovementDebugState {
    Vec3  velocity_vector     {};
    Vec3  wish_direction      {};
    Vec3  ground_normal       {};
    float jump_buffer_pct     = 0.0F;  // 0..1, how full the buffer is
    float coyote_pct          = 0.0F;
    float slide_pct           = 0.0F;
    bool  on_ground           = false;
    bool  on_ladder           = false;
    SurfaceMaterial ground_material = SurfaceMaterial::Default;
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Resolve a movement-state enum from the raw input command alone.
MovementState resolve_movement_state(const PlayerInputCommand& command);

/// Original direct-set movement simulation (kept for backward compat).
void simulate_player_movement(
    ReplicatedPlayerState& player_state,
    const PlayerInputCommand& command,
    float delta_seconds);

/// Quake / Source – style acceleration model with jump buffering and
/// coyote-time.  Operates purely on the replicated player state plus
/// a small persistent sim state so it is trivially testable without a
/// physics world.
void accelerate_movement(
    ReplicatedPlayerState& player_state,
    MovementSimState& sim_state,
    const PlayerInputCommand& command,
    float delta_seconds,
    const MovementConfig& config = MovementConfig{});

/// Determine whether a player is on the ground given a simple Y threshold.
[[nodiscard]] bool is_on_ground_simple(
    const ReplicatedPlayerState& player_state,
    float ground_y = 0.0F);

/// Compute the slope angle in degrees from a ground normal.
/// Returns 0 for a flat surface, 90 for a vertical wall.
[[nodiscard]] float compute_slope_angle(const Vec3& ground_normal);

/// Apply slide-down acceleration on a slope that exceeds max_walkable_slope.
/// Modifies velocity in-place.
void apply_slope_physics(
    Vec3& velocity,
    const Vec3& ground_normal,
    float delta_seconds,
    const MovementConfig& config);

/// Compute head-bob offset for this tick given current speed and phase.
/// Returns a Vec3 offset to add to the camera position.
[[nodiscard]] Vec3 compute_head_bob_offset(
    float current_speed,
    float max_speed,
    float& phase,
    float delta_seconds,
    const MovementConfig& config);

/// Compute a landing-impulse offset (one-shot downward jolt).
/// Call once per landing, returns the offset and resets internal state
/// externally by checking was_on_ground transitions.
[[nodiscard]] Vec3 compute_landing_impulse(
    float impact_speed,
    const MovementConfig& config);

/// Return a speed multiplier for the given surface material.
[[nodiscard]] float surface_speed_multiplier(
    SurfaceMaterial mat,
    const MovementConfig& config = MovementConfig{});

/// Return a friction multiplier for the given surface material.
[[nodiscard]] float surface_friction_multiplier(
    SurfaceMaterial mat,
    const MovementConfig& config = MovementConfig{});

}  // namespace ahamkara::game
