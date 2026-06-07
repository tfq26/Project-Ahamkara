#include "ahamkara/game/movement.h"

#include <algorithm>
#include <cmath>

namespace ahamkara::game {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

[[nodiscard]] float horizontal_length(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.z * v.z);
}

[[nodiscard]] float vec2_length(const Vec2& v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

[[nodiscard]] float clampf(float val, float lo, float hi) {
    return std::max(lo, std::min(val, hi));
}

[[nodiscard]] bool has_input(const Vec2& axis) {
    return std::fabs(axis.x) > 0.001F || std::fabs(axis.y) > 0.001F;
}

[[nodiscard]] float vec3_length(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

[[nodiscard]] Vec3 vec3_normalized(const Vec3& v) {
    float len = vec3_length(v);
    if (len < 0.000001F) return {0.0F, 1.0F, 0.0F};
    return {v.x / len, v.y / len, v.z / len};
}

[[nodiscard]] float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

}  // namespace

// ---------------------------------------------------------------------------
// resolve_movement_state
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// simulate_player_movement  (backward compat)
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// is_on_ground_simple
// ---------------------------------------------------------------------------

bool is_on_ground_simple(const ReplicatedPlayerState& player_state, float ground_y) {
    return player_state.position.y <= ground_y + 0.001F;
}

// ---------------------------------------------------------------------------
// compute_slope_angle  (area 32)
// ---------------------------------------------------------------------------

float compute_slope_angle(const Vec3& ground_normal) {
    // Angle between ground normal and world-up (0,1,0)
    // cos(theta) = dot(normal, up)
    float cos_angle = clampf(ground_normal.y, -1.0F, 1.0F);
    return std::acos(cos_angle) * (180.0F / 3.1415926535F);
}

// ---------------------------------------------------------------------------
// apply_slope_physics  (area 32)
// ---------------------------------------------------------------------------

void apply_slope_physics(
    Vec3& velocity,
    const Vec3& ground_normal,
    float delta_seconds,
    const MovementConfig& config) {
    float slope_deg = compute_slope_angle(ground_normal);
    if (slope_deg <= config.max_walkable_slope) return;

    // Compute downhill direction: project the ground normal onto the horizontal plane
    // and negate to get the downhill vector
    Vec3 downhill = {
        ground_normal.x,
        0.0F,
        ground_normal.z
    };
    float dh_len = vec3_length(downhill);
    if (dh_len < 0.0001F) return;

    downhill.x /= dh_len;
    downhill.z /= dh_len;

    // Extra acceleration proportional to how steep the slope is beyond the limit
    float excess = (slope_deg - config.max_walkable_slope) / 45.0F;  // 0..1
    float accel = config.slope_slide_accel * excess;
    velocity.x += downhill.x * accel * delta_seconds;
    velocity.z += downhill.z * accel * delta_seconds;

    // Also apply a downward push
    velocity.y -= accel * 0.5F * delta_seconds;
}

// ---------------------------------------------------------------------------
// surface_speed_multiplier  (area 40)
// ---------------------------------------------------------------------------

float surface_speed_multiplier(SurfaceMaterial mat, const MovementConfig& config) {
    int idx = static_cast<int>(mat);
    if (idx < 0 || idx >= static_cast<int>(SurfaceMaterial::Count)) return 1.0F;
    return config.surface_speed_mult[idx];
}

// ---------------------------------------------------------------------------
// surface_friction_multiplier  (area 40)
// ---------------------------------------------------------------------------

float surface_friction_multiplier(SurfaceMaterial mat, const MovementConfig& config) {
    int idx = static_cast<int>(mat);
    if (idx < 0 || idx >= static_cast<int>(SurfaceMaterial::Count)) return 1.0F;
    return config.surface_friction_mult[idx];
}

// ---------------------------------------------------------------------------
// compute_head_bob_offset  (area 38)
// ---------------------------------------------------------------------------

Vec3 compute_head_bob_offset(
    float current_speed,
    float max_speed,
    float& phase,
    float delta_seconds,
    const MovementConfig& config) {
    if (current_speed < 0.01F || max_speed < 0.01F) {
        // Return toward center
        phase = 0.0F;
        return {0.0F, 0.0F, 0.0F};
    }

    float speed_pct = clampf(current_speed / max_speed, 0.0F, 1.0F);
    phase += delta_seconds * config.head_bob_frequency * speed_pct;

    // Keep phase in [0, 2*pi)
    constexpr float kTwoPi = 2.0F * 3.1415926535F;
    phase = std::fmod(phase, kTwoPi);

    float vertical   = std::sin(phase * 2.0F) * config.head_bob_amplitude * speed_pct;
    float horizontal = std::cos(phase) * config.head_bob_amplitude * 0.5F * speed_pct;
    float roll       = std::sin(phase) * config.head_bob_roll_amount * speed_pct;

    // Return as Vec3 (x=horizontal sway, y=vertical bob, z=roll angle encoded)
    // Note: roll is returned in z for the caller to apply rotation
    return {horizontal, vertical, roll};
}

// ---------------------------------------------------------------------------
// compute_landing_impulse  (area 38)
// ---------------------------------------------------------------------------

Vec3 compute_landing_impulse(float impact_speed, const MovementConfig& config) {
    // impact_speed is the downward velocity at landing (positive = harder landing)
    float mag = impact_speed * config.landing_impulse_mag;
    return {0.0F, -mag, 0.0F};
}

// ---------------------------------------------------------------------------
// accelerate_movement  — full movement model (areas 31, 32, 34, 35, 40)
// ---------------------------------------------------------------------------

void accelerate_movement(
    ReplicatedPlayerState& player_state,
    MovementSimState& sim_state,
    const PlayerInputCommand& command,
    float delta_seconds,
    const MovementConfig& cfg) {
    if (delta_seconds <= 0.0F) return;

    // --- Tick slide cooldown (area 35) -----------------------------------
    if (sim_state.slide_cooldown_timer > 0.0F) {
        sim_state.slide_cooldown_timer = std::max(0.0F, sim_state.slide_cooldown_timer - delta_seconds);
    }

    // --- Determine ground state -------------------------------------------
    const bool on_ground = is_on_ground_simple(player_state, cfg.ground_y);

    // --- Surface material multipliers (area 40) ---------------------------
    float surf_speed_mult = surface_speed_multiplier(sim_state.ground_material, cfg);
    float surf_fric_mult  = surface_friction_multiplier(sim_state.ground_material, cfg);

    // --- Tick jump buffer timer -------------------------------------------
    if (command.jump_pressed) {
        sim_state.jump_buffer_timer = cfg.jump_buffer_time;
    } else if (sim_state.jump_buffer_timer > 0.0F) {
        sim_state.jump_buffer_timer = std::max(0.0F, sim_state.jump_buffer_timer - delta_seconds);
    }

    // --- Tick coyote timer ------------------------------------------------
    if (!on_ground && sim_state.was_on_ground) {
        sim_state.coyote_timer = cfg.coyote_time;
    } else if (sim_state.coyote_timer > 0.0F) {
        sim_state.coyote_timer = std::max(0.0F, sim_state.coyote_timer - delta_seconds);
    }

    // --- Resolve jump -----------------------------------------------------
    const bool can_jump = on_ground || (sim_state.coyote_timer > 0.0F);
    const bool want_jump = command.jump_pressed || (sim_state.jump_buffer_timer > 0.0F);
    bool did_jump = false;

    if (want_jump && can_jump) {
        player_state.velocity.y = cfg.jump_speed;
        sim_state.jump_buffer_timer = 0.0F;
        sim_state.coyote_timer = 0.0F;
        did_jump = true;
    }

    // --- Gravity -----------------------------------------------------------
    if (!on_ground && !did_jump) {
        player_state.velocity.y -= cfg.gravity * delta_seconds;
        float air_speed = vec3_length(player_state.velocity);
        if (air_speed > cfg.max_air_speed) {
            float scale = cfg.max_air_speed / air_speed;
            player_state.velocity.x *= scale;
            player_state.velocity.y *= scale;
            player_state.velocity.z *= scale;
        }
    } else if (!did_jump) {
        player_state.velocity.y = 0.0F;
        player_state.position.y = cfg.ground_y;
    }

    // --- Slope physics (area 32) — slide down steep slopes ----------------
    if (on_ground) {
        apply_slope_physics(player_state.velocity, sim_state.ground_normal, delta_seconds, cfg);
    }

    // --- Horizontal acceleration / friction --------------------------------
    const float input_magnitude = clampf(vec2_length(command.move_axis), 0.0F, 1.0F);
    float speed_cap = command.sprint_held ? cfg.sprint_speed : cfg.walk_speed;
    speed_cap *= surf_speed_mult;

    // Slide overrides speed cap (area 35)
    bool is_sliding = command.slide_pressed && on_ground && has_input(command.move_axis)
                      && sim_state.slide_cooldown_timer <= 0.0F;
    if (is_sliding) {
        speed_cap = cfg.slide_speed * surf_speed_mult;
        sim_state.slide_cooldown_timer = cfg.slide_cooldown;
    }

    if (input_magnitude > 0.001F) {
        const float inv_mag = 1.0F / input_magnitude;
        const float wish_x = command.move_axis.x * inv_mag;
        const float wish_z = command.move_axis.y * inv_mag;
        const float wish_speed = speed_cap * input_magnitude;

        const float current_speed = player_state.velocity.x * wish_x
                                  + player_state.velocity.z * wish_z;
        const float add_speed = wish_speed - current_speed;

        if (add_speed > 0.0F) {
            float accel_rate = on_ground ? cfg.ground_accel : cfg.air_accel;
            // Sprint accel multiplier (area 35)
            if (command.sprint_held && on_ground) {
                accel_rate *= cfg.sprint_accel_mult;
            }

            float accel_speed = accel_rate * delta_seconds * wish_speed;
            if (accel_speed > add_speed) accel_speed = add_speed;

            player_state.velocity.x += accel_speed * wish_x;
            player_state.velocity.z += accel_speed * wish_z;
        } else if (on_ground && command.sprint_held) {
            // Sharp turn penalty: lose some speed when wish direction differs
            // from current velocity direction (area 35)
            float h_speed = horizontal_length(player_state.velocity);
            if (h_speed > 0.5F) {
                float vel_dir_x = player_state.velocity.x / h_speed;
                float vel_dir_z = player_state.velocity.z / h_speed;
                float alignment = vel_dir_x * wish_x + vel_dir_z * wish_z;
                if (alignment < 0.7F) {  // sharp turn threshold
                    float penalty = cfg.sprint_turn_penalty;
                    player_state.velocity.x *= penalty;
                    player_state.velocity.z *= penalty;
                }
            }
        }
    } else {
        // --- No input — apply friction -----------------------------------
        const float h_speed = horizontal_length(player_state.velocity);
        if (h_speed > 0.001F) {
            float friction_rate = on_ground ? cfg.ground_friction : cfg.air_friction;
            friction_rate *= surf_fric_mult;
            float drop = friction_rate * delta_seconds * h_speed;
            if (drop > h_speed) drop = h_speed;
            float scale = (h_speed - drop) / h_speed;
            player_state.velocity.x *= scale;
            player_state.velocity.z *= scale;
        }
    }

    // --- Integrate position ------------------------------------------------
    player_state.position.x += player_state.velocity.x * delta_seconds;
    player_state.position.z += player_state.velocity.z * delta_seconds;
    player_state.position.y += player_state.velocity.y * delta_seconds;

    // --- Clamp to ground ---------------------------------------------------
    if (player_state.position.y < cfg.ground_y) {
        player_state.position.y = cfg.ground_y;
        if (player_state.velocity.y < 0.0F) {
            player_state.velocity.y = 0.0F;
        }
    }

    // --- Update movement state enum ---------------------------------------
    if (sim_state.ledge_grabbed) {
        player_state.movement_state = MovementState::LedgeGrab;
    } else if (sim_state.is_mantling) {
        player_state.movement_state = MovementState::Mantling;
    } else if (sim_state.on_ladder) {
        player_state.movement_state = MovementState::OnLadder;
    } else if (!on_ground || player_state.velocity.y > 0.001F) {
        player_state.movement_state = MovementState::Jumping;
    } else if (is_sliding) {
        player_state.movement_state = MovementState::Sliding;
    } else if (command.sprint_held && has_input(command.move_axis)) {
        player_state.movement_state = MovementState::Sprinting;
    } else if (has_input(command.move_axis)) {
        player_state.movement_state = MovementState::Walking;
    } else {
        player_state.movement_state = MovementState::Idle;
    }

    // --- Persist ground state for next tick -------------------------------
    sim_state.was_on_ground = on_ground;
}

}  // namespace ahamkara::game
