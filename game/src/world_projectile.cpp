#include "world_projectile.h"
#include "world_jolt_bridge.h"

#include "ahamkara/game/movement.h"
#include "ae/core/math.h"

#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Body/Body.h>

#include <algorithm>
#include <cmath>

namespace ahamkara::game {
namespace {

constexpr float kProjectileSpeed = 80.0F;
constexpr float kProjectileLifetime = 2.0F;
constexpr float kFireCooldown = 0.1F;
constexpr float kAimAssistRadius = 1.5F;        // degrees
constexpr float kAimAssistMaxAngle = 3.0F;       // degrees
constexpr float kDummyHitRadius = 0.5F;
constexpr float kBaseDamage = 25.0F;
constexpr float kHeadshotMultiplier = 2.0F;
constexpr float kHeadshotHeightOffset = 0.15F;   // above dummy center

struct AimAssistTarget {
    Vec3 position {};
    float score {0.0F};
};

// Compute yaw/pitch angles from origin to target
void compute_aim_angles(const Vec3& origin, const Vec3& target, float& out_yaw, float& out_pitch) {
    float dx = target.x - origin.x;
    float dy = target.y - origin.y;
    float dz = target.z - origin.z;
    float h_dist = std::sqrt(dx * dx + dz * dz);
    out_yaw = std::atan2(dx, dz) * (180.0F / 3.1415926535F);
    out_pitch = -std::atan2(dy, h_dist) * (180.0F / 3.1415926535F);
}

[[nodiscard]] float angle_difference(float a, float b) {
    float diff = std::fmod(a - b + 540.0F, 360.0F);
    if (diff > 180.0F) diff -= 360.0F;
    return diff;
}

}  // namespace

// --- fire_projectile ----------------------------------------------------------

void fire_projectile(World& world, const PlayerInputCommand& input) {
    (void)input;

    // Rate of fire check via ammo
    if (world.get_ammo_current() <= 0) return;
    world.set_fire_cooldown_timer(world.fire_cooldown_timer() - 1.0F / 60.0F);
    // NOTE: actual cooldown managed by world tick; this is a simplified check

    const auto& player_state = world.get_player_state();
    const auto& camera_anchor = world.get_camera_anchor();

    // Eye/camera position for projectile origin
    Vec3 origin = camera_anchor.position;
    origin.y += (player_state.position.y > 0.0F ? 0.58F : 0.58F); // eye height offset

    // Compute forward direction from camera yaw/pitch
    float yaw_rad = ae::to_radians(camera_anchor.yaw);
    float pitch_rad = ae::to_radians(camera_anchor.pitch);
    Vec3 forward {
        std::sin(yaw_rad) * std::cos(pitch_rad),
        -std::sin(pitch_rad),
        std::cos(yaw_rad) * std::cos(pitch_rad)
    };

    // Aim assist magnetism: find the nearest dummy within the assist cone
    Vec3 aim_dir = forward;
    const TargetDummyState* dummies = world.get_dummies();
    int dummy_count = world.get_dummy_count();

    AimAssistTarget best_target {};
    best_target.score = 9999.0F;
    bool found_target = false;

    for (int i = 0; i < dummy_count; ++i) {
        const auto& d = dummies[i];
        if (!d.alive) continue;

        // Target center point (dummy is a capsule centered at position)
        Vec3 target_pos = d.position;
        target_pos.y += 1.0F; // top of capsule

        float target_yaw, target_pitch;
        compute_aim_angles(origin, target_pos, target_yaw, target_pitch);

        float yaw_diff = std::fabs(angle_difference(camera_anchor.yaw, target_yaw));
        float pitch_diff = std::fabs(camera_anchor.pitch - target_pitch);

        if (yaw_diff < kAimAssistMaxAngle && pitch_diff < kAimAssistMaxAngle) {
            float score = yaw_diff * 1.0F + pitch_diff * 1.5F;
            if (score < best_target.score) {
                best_target.score = score;
                best_target.position = target_pos;
                found_target = true;
            }
        }
    }

    if (found_target) {
        // Magnet: adjust aim direction toward the target (partial lock)
        Vec3 to_target {
            best_target.position.x - origin.x,
            best_target.position.y - origin.y,
            best_target.position.z - origin.z
        };
        float len = std::sqrt(to_target.x * to_target.x + to_target.y * to_target.y + to_target.z * to_target.z);
        if (len > 0.001F) {
            to_target.x /= len;
            to_target.y /= len;
            to_target.z /= len;
            // Blend: 60% toward target, 40% original
            constexpr float magnetism = 0.6F;
            aim_dir.x = aim_dir.x * (1.0F - magnetism) + to_target.x * magnetism;
            aim_dir.y = aim_dir.y * (1.0F - magnetism) + to_target.y * magnetism;
            aim_dir.z = aim_dir.z * (1.0F - magnetism) + to_target.z * magnetism;
            // Re-normalize
            float alen = std::sqrt(aim_dir.x * aim_dir.x + aim_dir.y * aim_dir.y + aim_dir.z * aim_dir.z);
            if (alen > 0.001F) {
                aim_dir.x /= alen;
                aim_dir.y /= alen;
                aim_dir.z /= alen;
            }
        }
    }

    // Find free projectile slot (mutable access via World::projectiles_mut())
    auto* projectiles = world.projectiles_mut();

    int proj_count = world.get_projectile_count();
    if (proj_count >= world.get_max_projectiles()) return;

    auto& proj = projectiles[proj_count];
    proj.position = origin;
    proj.position.x += aim_dir.x * 0.3F; // spawn slightly in front
    proj.position.y += aim_dir.y * 0.3F;
    proj.position.z += aim_dir.z * 0.3F;
    proj.velocity = {
        aim_dir.x * kProjectileSpeed,
        aim_dir.y * kProjectileSpeed,
        aim_dir.z * kProjectileSpeed
    };
    proj.lifetime_seconds = kProjectileLifetime;
    proj.alive = true;
    proj.first_tick = true;
    proj.client_tick = 0;

    // Bump projectile count
    world.set_projectile_count(proj_count + 1);

    // Muzzle flash feedback
    world.set_muzzle_flash(0.05F);
    world.spawn_muzzle_particles(origin, aim_dir);

    // Audio: fire sound
    world.queue_audio_event(AudioEvent{"weapon_fire", 1.0f, AudioCategory::Weapon});
}

// --- step_projectiles ----------------------------------------------------------

void step_projectiles(World& world, float delta_seconds) {
    const auto* dummies = world.get_dummies();
    int dummy_count = world.get_dummy_count();
    auto* projectiles = world.projectiles_mut();
    int proj_count = world.get_projectile_count();
    int max_proj = world.get_max_projectiles();

    (void)max_proj;

    int active = 0;

    for (int i = 0; i < proj_count; ++i) {
        auto& p = projectiles[i];

        if (!p.alive) continue;

        p.lifetime_seconds -= delta_seconds;
        if (p.lifetime_seconds <= 0.0F) {
            p.alive = false;
            continue;
        }

        p.first_tick = false;

        // Move projectile
        Vec3 prev_pos = p.position;
        p.position.x += p.velocity.x * delta_seconds;
        p.position.y += p.velocity.y * delta_seconds;
        p.position.z += p.velocity.z * delta_seconds;

        // Raycast between prev and current position for dummy hit detection
        Vec3 ray_start = prev_pos;
        Vec3 ray_dir {
            p.position.x - prev_pos.x,
            p.position.y - prev_pos.y,
            p.position.z - prev_pos.z
        };
        float ray_len = std::sqrt(ray_dir.x * ray_dir.x + ray_dir.y * ray_dir.y + ray_dir.z * ray_dir.z);

        bool hit_something = false;
        Vec3 hit_point = p.position;
        int hit_dummy_idx = -1;
        bool is_headshot = false;

        // Check against each alive dummy
        if (ray_len > 0.001F) {
            float inv_len = 1.0F / ray_len;
            ray_dir.x *= inv_len;
            ray_dir.y *= inv_len;
            ray_dir.z *= inv_len;

            float closest_t = ray_len;
            for (int d = 0; d < dummy_count; ++d) {
                const auto& dummy = dummies[d];
                if (!dummy.alive) continue;

                // Dummy capsule: center at position, radius 0.35, height ~2.0
                Vec3 d_center = dummy.position;
                float d_radius = 0.35F;
                float d_half_height = 1.0F;
                float d_top = d_center.y + d_half_height;
                float d_bottom = d_center.y - d_half_height;

                // Broad phase: sphere at dummy center
                Vec3 to_center {
                    d_center.x - ray_start.x,
                    d_center.y - ray_start.y,
                    d_center.z - ray_start.z
                };
                float proj_dist = to_center.x * ray_dir.x + to_center.y * ray_dir.y + to_center.z * ray_dir.z;
                if (proj_dist < 0.0F || proj_dist > ray_len + d_radius) continue;

                float closest_sq = 0.0F;
                {
                    float dx = ray_start.x + ray_dir.x * proj_dist - d_center.x;
                    float dy = ray_start.y + ray_dir.y * proj_dist - d_center.y;
                    float dz = ray_start.z + ray_dir.z * proj_dist - d_center.z;
                    closest_sq = dx * dx + dy * dy + dz * dz;
                }
                float max_r = d_radius + 0.05F; // projectile radius
                if (closest_sq > max_r * max_r) continue;

                // Refined capsule check
                float cy = d_center.y;
                float t = proj_dist;

                // Check body hit
                float bx = ray_start.x + ray_dir.x * t - d_center.x;
                float by = ray_start.y + ray_dir.y * t - cy;
                float bz = ray_start.z + ray_dir.z * t - d_center.z;
                float clamped_final_y = std::max(-d_half_height, std::min(d_half_height, by));
                float dist_sq = bx * bx + (by - clamped_final_y) * (by - clamped_final_y) + bz * bz;
                float hit_r = d_radius + 0.03F;
                if (dist_sq <= hit_r * hit_r) {
                    if (t > 0.0F && t < closest_t) {
                        closest_t = t;
                        hit_dummy_idx = d;
                        hit_something = true;
                        // Headshot check: hit above 80% of dummy height
                        float hit_y = ray_start.y + ray_dir.y * t;
                        float head_y = d_center.y + d_half_height - 0.3F;
                        is_headshot = (hit_y >= head_y);
                    }
                }
            }

            if (hit_something) {
                hit_point.x = ray_start.x + ray_dir.x * closest_t;
                hit_point.y = ray_start.y + ray_dir.y * closest_t;
                hit_point.z = ray_start.z + ray_dir.z * closest_t;
                p.position = hit_point;
                p.alive = false;
                
                Vec3 normal = { -ray_dir.x, -ray_dir.y, -ray_dir.z };
                float nlen = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
                if (nlen > 0.001F) {
                    normal.x /= nlen; normal.y /= nlen; normal.z /= nlen;
                }
                world.spawn_impact_particles(hit_point, normal);
                world.spawn_bullet_hole_decal(hit_point, normal);
            }
        }

        if (hit_something && hit_dummy_idx >= 0) {
            // Deal damage directly to the dummy (mutable access via World::dummies_mut())
            TargetDummyState* mutable_dummies = world.dummies_mut();
            auto& dummy = mutable_dummies[hit_dummy_idx];

            float damage = is_headshot ? kBaseDamage * kHeadshotMultiplier : kBaseDamage;
            dummy.health -= damage;
            dummy.last_hit_timer = 0.3F;
            dummy.was_hit_precision = is_headshot;
            dummy.last_damage_dealt = damage;
            dummy.last_hit_position = hit_point;

            if (dummy.health <= 0.0F) {
                dummy.health = 0.0F;
                dummy.alive = false;
                dummy.respawn_timer = 3.0F;
            }

            // Spawn damage number
            Vec3 num_pos = hit_point;
            num_pos.y += 0.3F;
            world.spawn_damage_number(num_pos, damage, is_headshot);

            // Hitmarker
            world.set_hitmarker(0.15F, is_headshot);

            // Hit sound
            world.queue_audio_event(AudioEvent{"dummy_hit", 1.0f, AudioCategory::SFX});
        }

        // Compact alive projectiles to front
        if (!p.alive) continue;
        if (active != i) {
            projectiles[active] = p;
        }
        active++;
    }

    // Update projectile count
    world.set_projectile_count(active);
}

}  // namespace ahamkara::game
