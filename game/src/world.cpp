#include "ahamkara/game/world.h"

#include "ahamkara/game/movement.h"
#include "ae/core/math.h"

#include <algorithm>
#include <cmath>

namespace ahamkara::game {
namespace {

constexpr float kGroundHeight = 0.0F;
constexpr float kJumpSpeed = 5.5F;
constexpr float kGravity = 18.0F;
constexpr float kWalkSpeed = 3.0F;
constexpr float kSprintSpeed = 6.0F;
constexpr float kSlideSpeed = 10.0F;
constexpr float kSlideDurationSeconds = 0.45F;
constexpr float kStandingEyeHeight = 0.58F;
constexpr float kCrouchingEyeHeight = 0.32F;
constexpr float kStandingVisualHeight = 0.65F;
constexpr float kCrouchingVisualHeight = 0.35F;
constexpr float kPlayerCollisionRadius = 0.22F;

bool has_move_input(const PlayerInputCommand& input) {
    return std::fabs(input.move_axis.x) > 0.001F || std::fabs(input.move_axis.y) > 0.001F;
}

}  // namespace

World::World() {
    player_state_ = {};
    player_state_.position.x = -12.0F;  // Alpha spawn
    camera_anchor_ = {};
    camera_anchor_.position.x = -12.0F;
    colliders_ = kDebugMapColliders.data();
    collider_count_ = kDebugMapColliders.size();
}

void World::tick(float delta_seconds, const PlayerInputCommand& input) {
    slide_timer_seconds_ = std::max(0.0F, slide_timer_seconds_ - delta_seconds);
    if (input.slide_pressed && is_on_ground() && has_move_input(input)) {
        slide_timer_seconds_ = kSlideDurationSeconds;
    }

    crouch_active_ = input.crouch_held || slide_timer_seconds_ > 0.0F;

    update_horizontal_motion(delta_seconds, input);
    update_vertical_motion(delta_seconds, input);
    update_movement_state(input);

    player_state_.yaw += input.look_delta.x;
    update_camera(input);

    // Debug free-fire: no ammo depletion or reload gating for now.
    ammo_current_ = ammo_max_;

    if (input.fire_held) {
        spawn_projectile();
    }

    update_projectiles(delta_seconds);
}

void World::set_player_state(const ReplicatedPlayerState& state) {
    player_state_ = state;
    update_camera(PlayerInputCommand {});
}

float World::get_player_visual_height() const {
    return crouch_active_ ? kCrouchingVisualHeight : kStandingVisualHeight;
}

bool World::is_on_ground() const {
    if (player_state_.position.y <= kGroundHeight + 0.0001F) {
        return true;
    }
    for (std::size_t i = 0; i < collider_count_; ++i) {
        const auto& c = colliders_[i];
        if (player_state_.position.x + kPlayerCollisionRadius >= c.min_x
            && player_state_.position.x - kPlayerCollisionRadius <= c.max_x
            && player_state_.position.z + kPlayerCollisionRadius >= c.min_z
            && player_state_.position.z - kPlayerCollisionRadius <= c.max_z) {
            if (std::fabs(player_state_.position.y - c.top_y) <= 0.001F) {
                return true;
            }
        }
    }
    return false;
}

void World::update_horizontal_motion(float delta_seconds, const PlayerInputCommand& input) {
    float move_speed = kWalkSpeed;
    if (slide_timer_seconds_ > 0.0F) {
        move_speed = kSlideSpeed;
    } else if (input.sprint_held) {
        move_speed = kSprintSpeed;
    }

    // Rotate movement input by player yaw for camera-relative movement
    const float yaw_rad = ae::to_radians(player_state_.yaw);
    const float forward_x = std::sin(yaw_rad);
    const float forward_z = std::cos(yaw_rad);
    const float right_x = std::cos(yaw_rad);
    const float right_z = -std::sin(yaw_rad);

    player_state_.velocity.x = (input.move_axis.x * right_x + input.move_axis.y * forward_x) * move_speed;
    player_state_.velocity.z = (input.move_axis.x * right_z + input.move_axis.y * forward_z) * move_speed;
    player_state_.position.x += player_state_.velocity.x * delta_seconds;
    player_state_.position.z += player_state_.velocity.z * delta_seconds;

    resolve_horizontal_collisions();
}

void World::update_vertical_motion(float delta_seconds, const PlayerInputCommand& input) {
    if (input.jump_pressed && is_on_ground() && slide_timer_seconds_ <= 0.0F) {
        player_state_.velocity.y = kJumpSpeed;
    }

    if (!is_on_ground() || player_state_.velocity.y > 0.0F) {
        player_state_.velocity.y -= kGravity * delta_seconds;
        player_state_.position.y += player_state_.velocity.y * delta_seconds;
    }

    // Snap to platform tops before the ground clamp
    resolve_platform_collisions();

    // Mantle: pull up onto nearby platform edges while jumping
    resolve_mantle();

    if (player_state_.position.y <= kGroundHeight) {
        player_state_.position.y = kGroundHeight;
        if (player_state_.velocity.y < 0.0F) {
            player_state_.velocity.y = 0.0F;
        }
    }
}

void World::update_movement_state(const PlayerInputCommand& input) {
    if (!is_on_ground() || player_state_.velocity.y > 0.001F) {
        player_state_.movement_state = MovementState::Jumping;
        return;
    }

    if (slide_timer_seconds_ > 0.0F) {
        player_state_.movement_state = MovementState::Sliding;
        return;
    }

    if (input.sprint_held && has_move_input(input)) {
        player_state_.movement_state = MovementState::Sprinting;
        return;
    }

    if (has_move_input(input)) {
        player_state_.movement_state = MovementState::Walking;
        return;
    }

    player_state_.movement_state = MovementState::Idle;
}

void World::set_colliders(const ColliderBox* colliders, std::size_t count) {
    colliders_ = colliders;
    collider_count_ = count;
}

void World::resolve_platform_collisions() {
    const float feet_y = player_state_.position.y;
    const float px = player_state_.position.x;
    const float pz = player_state_.position.z;
    const bool falling = player_state_.velocity.y <= 0.0F;

    for (std::size_t i = 0; i < collider_count_; ++i) {
        const auto& c = colliders_[i];
        if (px < c.min_x || px > c.max_x || pz < c.min_z || pz > c.max_z) {
            continue;
        }

        // For platforms with auto_step disabled, only resolve when
        // actively jumping or falling onto the platform from above
        if (!c.auto_step && player_state_.velocity.y <= 0.1F && feet_y <= c.top_y) {
            continue;
        }

        if (c.jump_through) {
            // Jump-through: only snap when landing from above the surface
            if (feet_y >= c.top_y && feet_y <= c.top_y + 0.6F && falling) {
                player_state_.position.y = c.top_y;
                player_state_.velocity.y = 0.0F;
                return;
            }
        } else {
            // Solid platform: snap when feet are within the volume and falling
            if (feet_y <= c.top_y && feet_y >= c.bottom_y && falling) {
                player_state_.position.y = c.top_y;
                if (player_state_.velocity.y < 0.0F) {
                    player_state_.velocity.y = 0.0F;
                }
                return;
            }

            // Player is just above the platform and falling — snap down
            if (feet_y > c.top_y && feet_y <= c.top_y + 0.6F && falling) {
                player_state_.position.y = c.top_y;
                player_state_.velocity.y = 0.0F;
                return;
            }
        }
    }
}

void World::resolve_horizontal_collisions() {
    float px = player_state_.position.x;
    float pz = player_state_.position.z;
    const float feet_y = player_state_.position.y;

    for (std::size_t i = 0; i < collider_count_; ++i) {
        const auto& c = colliders_[i];
        if (!c.wall) {
            continue;
        }

        const float expanded_min_x = c.min_x - kPlayerCollisionRadius;
        const float expanded_max_x = c.max_x + kPlayerCollisionRadius;
        const float expanded_min_z = c.min_z - kPlayerCollisionRadius;
        const float expanded_max_z = c.max_z + kPlayerCollisionRadius;

        if (px <= expanded_min_x || px >= expanded_max_x || pz <= expanded_min_z || pz >= expanded_max_z) {
            continue;
        }

        if (feet_y < c.bottom_y || feet_y > c.top_y + 0.6F) {
            continue;
        }

        const float dx_min = px - expanded_min_x;
        const float dx_max = expanded_max_x - px;
        const float dz_min = pz - expanded_min_z;
        const float dz_max = expanded_max_z - pz;

        const float min_pen = std::min({dx_min, dx_max, dz_min, dz_max});

        if (min_pen == dx_min) {
            px = expanded_min_x;
            player_state_.velocity.x = 0.0F;
        } else if (min_pen == dx_max) {
            px = expanded_max_x;
            player_state_.velocity.x = 0.0F;
        } else if (min_pen == dz_min) {
            pz = expanded_min_z;
            player_state_.velocity.z = 0.0F;
        } else {
            pz = expanded_max_z;
            player_state_.velocity.z = 0.0F;
        }
    }

    player_state_.position.x = px;
    player_state_.position.z = pz;
}

void World::resolve_mantle() {
    // Only trigger when actively jumping upward
    if (player_state_.velocity.y <= 0.5F) return;

    const float feet_y = player_state_.position.y;
    const float eye_y = feet_y + (crouch_active_ ? kCrouchingEyeHeight : kStandingEyeHeight);
    const float px = player_state_.position.x;
    const float pz = player_state_.position.z;
    constexpr float mantle_margin = 0.3F;

    for (std::size_t i = 0; i < collider_count_; ++i) {
        const auto& c = colliders_[i];
        if (c.wall || c.jump_through || !c.auto_step) continue;

        // Check if player is near the platform horizontally
        const bool in_x = px >= c.min_x - mantle_margin && px <= c.max_x + mantle_margin;
        const bool in_z = pz >= c.min_z - mantle_margin && pz <= c.max_z + mantle_margin;
        if (!in_x || !in_z) continue;

        // Player's eye is above the platform top (can see over the edge)
        if (eye_y < c.top_y) continue;

        // Feet are within mantle range below the platform top
        const float dist_below = c.top_y - feet_y;
        if (dist_below < 0.2F || dist_below > 1.3F) continue;

        // Mantle! Pull player up onto the platform
        player_state_.position.y = c.top_y;
        player_state_.velocity.y = 0.0F;
        return;
    }
}

void World::spawn_projectile() {
    const float eye_y = player_state_.position.y
        + (crouch_active_ ? kCrouchingEyeHeight : kStandingEyeHeight);
    const float yaw_rad = std::atan2(
        std::sin(camera_anchor_.yaw * 3.14159265F / 180.0F),
        std::cos(camera_anchor_.yaw * 3.14159265F / 180.0F));
    const float pitch_rad = camera_anchor_.pitch * 3.14159265F / 180.0F;
    const float cos_pitch = std::cos(pitch_rad);

    Vec3 forward {
        std::sin(yaw_rad) * cos_pitch,
        std::sin(pitch_rad),
        std::cos(yaw_rad) * cos_pitch,
    };

    ProjectileState* projectile = nullptr;
    if (projectile_count_ < kMaxProjectiles) {
        projectile = &projectiles_[projectile_count_++];
    } else {
        // Buffer is full — find the first dead slot, or fall back to
        // the projectile with the shortest remaining lifetime so we
        // never lose a fresh projectile mid-flight.
        int best_slot = 0;
        float best_lifetime = projectiles_[0].lifetime_seconds;
        for (int i = 0; i < kMaxProjectiles; ++i) {
            if (!projectiles_[i].alive) {
                best_slot = i;
                break;
            }
            if (projectiles_[i].lifetime_seconds < best_lifetime) {
                best_lifetime = projectiles_[i].lifetime_seconds;
                best_slot = i;
            }
        }
        projectile = &projectiles_[best_slot];
    }

    ProjectileState& p = *projectile;
    p.position.x = camera_anchor_.position.x + forward.x * 0.5F;
    p.position.y = eye_y + forward.y * 0.3F;
    p.position.z = camera_anchor_.position.z + forward.z * 0.5F;

    constexpr float projectile_speed = 60.0F;
    p.velocity.x = forward.x * projectile_speed;
    p.velocity.y = forward.y * projectile_speed;
    p.velocity.z = forward.z * projectile_speed;
    p.lifetime_seconds = 2.0F;
    p.alive = true;
}

void World::update_projectiles(float delta_seconds) {
    for (int i = 0; i < projectile_count_; ++i) {
        auto& p = projectiles_[i];
        if (!p.alive) continue;

        p.lifetime_seconds -= delta_seconds;
        if (p.lifetime_seconds <= 0.0F) {
            p.alive = false;
            continue;
        }

        p.position.x += p.velocity.x * delta_seconds;
        p.position.y += p.velocity.y * delta_seconds;
        p.position.z += p.velocity.z * delta_seconds;

        // Check collision with colliders
        for (std::size_t j = 0; j < collider_count_; ++j) {
            const auto& c = colliders_[j];
            if (p.position.x >= c.min_x && p.position.x <= c.max_x
                && p.position.z >= c.min_z && p.position.z <= c.max_z
                && p.position.y >= c.bottom_y && p.position.y <= c.top_y) {
                p.alive = false;
                break;
            }
        }

        // Check ground collision
        if (p.position.y <= 0.0F) {
            p.alive = false;
        }
    }

    // Compact dead projectiles
    int write = 0;
    for (int i = 0; i < projectile_count_; ++i) {
        if (projectiles_[i].alive) {
            if (write != i) projectiles_[write] = projectiles_[i];
            ++write;
        }
    }
    projectile_count_ = write;
}

void World::update_camera(const PlayerInputCommand& input) {
    camera_anchor_.position = player_state_.position;
    camera_anchor_.position.y += crouch_active_ ? kCrouchingEyeHeight : kStandingEyeHeight;

    camera_anchor_.yaw = std::fmod(player_state_.yaw, 360.0F);
    if (camera_anchor_.yaw >= 180.0F) {
        camera_anchor_.yaw -= 360.0F;
    }
    if (camera_anchor_.yaw < -180.0F) {
        camera_anchor_.yaw += 360.0F;
    }
    camera_anchor_.pitch += input.look_delta.y;
    camera_anchor_.pitch = std::clamp(camera_anchor_.pitch, -89.0F, 89.0F);
}

}  // namespace ahamkara::game
