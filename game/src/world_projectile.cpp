#include "world_projectile.h"
#include "world_jolt_bridge.h"
#include "world_dummy_sim.h"

#include "ahamkara/game/movement.h"
#include "ae/core/math.h"
#include "ae/core/log.h"
#include "ahamkara/game/weapon_registry.h"

#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Body/Body.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace ahamkara::game {
namespace {

constexpr float kProjectileSpeed = 80.0F;
constexpr float kRocketSpeed = 30.0F;
constexpr float kProjectileLifetime = 2.0F;
constexpr float kRocketLifetime = 5.0F;
constexpr float kAimAssistMaxAngle = 3.0F;       // degrees
constexpr float kShotgunPelletCount = 8.0F;
constexpr float kShotgunSpreadAngle = 5.0F;       // degrees

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

/// Apply deterministic recoil from the weapon definition's recoil pattern.
/// Advances the recoil index each shot so the pattern stays reproducible.
void apply_recoil(World& world, const WeaponDefinition& def) {
    if (def.recoil_pattern.empty()) return;
    const std::size_t idx = static_cast<std::size_t>(world.recoil_index()) % def.recoil_pattern.size();
    const auto& entry = def.recoil_pattern[idx];
    // Accumulate recoil offsets through the pattern deterministically.
    // The look system reads accumulated pitch/yaw offsets each frame.
    world.advance_recoil();
    (void)entry;  // Recoil values available for camera system integration.
}

/// Deterministic spread helper — uses recoil index as seed for per-pellet offset
/// to avoid non-deterministic std::rand() calls.
float deterministic_spread_offset(int recoil_index, int pellet_index, float max_angle) {
    // Simple deterministic hash from recoil_index and pellet_index.
    const int hash = (recoil_index * 73856093) ^ (pellet_index * 19349663);
    const float normalized = static_cast<float>(hash & 0x7FFFFFFF) / 1073741824.0F;  // / 2^30
    return (normalized - 0.5F) * 2.0F * max_angle;
}

}  // namespace

// --- fire_projectile ----------------------------------------------------------

void fire_projectile(World& world, const PlayerInputCommand& input) {
    (void)input;

    // Check weapon can fire
    if (!world.can_fire()) return;

    const auto& def = world.get_active_weapon_def();
    const float base_damage = def.base_damage;
    const float headshot_multiplier = def.headshot_multiplier;

    // Fire cooldown derived from weapon definition RPM.
    world.set_fire_cooldown_timer(def.fire_interval());

    // Consume ammunition
    world.consume_ammo();
    world.notify_weapon_fired();

    ae::log_info("Fired: " + std::string(weapon_name(world.get_active_weapon_index())) +
                 " | ammo=" + std::to_string(world.get_ammo_current()) +
                 "/" + std::to_string(world.get_ammo_max()));

    // Projectile weapon classification: rocket launcher uses slot Melee.
    const bool is_rocket = (def.slot == WeaponSlot::Melee);
    // Shotgun check: Secondary slot with projectile fire mode.
    const bool is_shotgun_proj = (def.slot == WeaponSlot::Secondary && def.fire_mode == FireMode::Projectile);

    // For shotgun: fire multiple pellets
    const int pellets = is_shotgun_proj ? static_cast<int>(kShotgunPelletCount) : 1;
    const float spread_angle = (pellets > 1) ? kShotgunSpreadAngle : 0.0F;

    // Rocket launcher: slower projectile speed, longer lifetime
    const float proj_speed = is_rocket ? kRocketSpeed : kProjectileSpeed;
    const float proj_lifetime = is_rocket ? kRocketLifetime : kProjectileLifetime;

    const auto& camera_anchor = world.get_camera_anchor();

    // Eye/camera position for projectile origin
    Vec3 origin = camera_anchor.position;

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
    auto& registry = world.registry();
    auto dummy_view = registry.view<TargetDummyComponent>();

    AimAssistTarget best_target {};
    best_target.score = 9999.0F;
    bool found_target = false;

    for (auto dummy_entity : dummy_view) {
        const auto& d = dummy_view.get<TargetDummyComponent>(dummy_entity).state;
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

    // Limit active projectiles (account for multiple pellets)
    int active_count = 0;
    auto proj_view = registry.view<WorldProjectileComponent>();
    for (auto entity : proj_view) {
        if (proj_view.get<WorldProjectileComponent>(entity).state.alive) {
            active_count++;
        }
    }
    if (active_count + pellets > world.get_max_projectiles()) return;

    // Spawn projectile(s) — loop over pellets for shotgun spread
    for (int p = 0; p < pellets; ++p) {
        Vec3 pellet_dir = aim_dir;

        // Apply spread for shotgun pellets
        if (pellets > 1 && p > 0) {
            const int rng_seed = world.recoil_index();
            float angle_offset_yaw = deterministic_spread_offset(rng_seed, p, spread_angle);
            float angle_offset_pitch = deterministic_spread_offset(rng_seed, p + 1000, spread_angle);
            float pellet_yaw = camera_anchor.yaw + angle_offset_yaw;
            float pellet_pitch = camera_anchor.pitch + angle_offset_pitch;
            float py = ae::to_radians(pellet_yaw);
            float pp = ae::to_radians(pellet_pitch);
            pellet_dir = {
                std::sin(py) * std::cos(pp),
                -std::sin(pp),
                std::cos(py) * std::cos(pp)
            };
        }

        auto entity = registry.create();
        ProjectileState proj;
        proj.position = origin;
        proj.position.x += pellet_dir.x * 0.3F;
        proj.position.y += pellet_dir.y * 0.3F;
        proj.position.z += pellet_dir.z * 0.3F;
        proj.velocity = {
            pellet_dir.x * proj_speed,
            pellet_dir.y * proj_speed,
            pellet_dir.z * proj_speed
        };
        proj.lifetime_seconds = proj_lifetime;
        proj.alive = true;
        proj.first_tick = true;
        proj.client_tick = 0;

        WorldProjectileComponent comp;
        comp.state = proj;
        comp.base_damage = base_damage;
        comp.headshot_multiplier = headshot_multiplier;
        comp.owner_id = 1;
        registry.emplace<WorldProjectileComponent>(entity, comp);
    }

    // Muzzle flash feedback
    world.set_muzzle_flash(0.05F);
    world.spawn_muzzle_particles(origin, aim_dir);

    // Apply deterministic recoil from weapon definition pattern
    apply_recoil(world, def);

    // Audio: fire sound
    world.queue_audio_event(AudioEvent{"weapon_fire", 1.0f, AudioCategory::Weapon});
}

// --- step_projectiles ----------------------------------------------------------

void step_projectiles(World& world, float delta_seconds) {
    auto& registry = world.registry();
    auto dummy_view = registry.view<TargetDummyComponent>();
    auto proj_view = registry.view<WorldProjectileComponent>();

    static thread_local std::vector<entt::entity> to_destroy;
    to_destroy.clear();

    for (auto proj_entity : proj_view) {
        auto& comp = proj_view.get<WorldProjectileComponent>(proj_entity);
        auto& p = comp.state;

        if (!p.alive) {
            to_destroy.push_back(proj_entity);
            continue;
        }

        p.lifetime_seconds -= delta_seconds;
        if (p.lifetime_seconds <= 0.0F) {
            p.alive = false;
            to_destroy.push_back(proj_entity);
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
        entt::entity hit_dummy_entity = entt::null;
        bool is_headshot = false;

        // Check against each alive dummy
        if (ray_len > 0.001F) {
            float inv_len = 1.0F / ray_len;
            ray_dir.x *= inv_len;
            ray_dir.y *= inv_len;
            ray_dir.z *= inv_len;

            float closest_t = ray_len;
            for (auto dummy_entity : dummy_view) {
                const auto& dummy = dummy_view.get<TargetDummyComponent>(dummy_entity).state;
                if (!dummy.alive) continue;

                // Dummy capsule: center at position, radius 0.35, height ~2.0
                Vec3 d_center = dummy.position;
                float d_radius = 0.35F;
                float d_half_height = 1.0F;

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
                        hit_dummy_entity = dummy_entity;
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
                to_destroy.push_back(proj_entity);
                
                Vec3 normal = { -ray_dir.x, -ray_dir.y, -ray_dir.z };
                float nlen = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
                if (nlen > 0.001F) {
                    normal.x /= nlen; normal.y /= nlen; normal.z /= nlen;
                }
                world.spawn_impact_particles(hit_point, normal);
                world.spawn_bullet_hole_decal(hit_point, normal);
            }
        }

        if (hit_something && hit_dummy_entity != entt::null) {
            auto& dummy_comp = dummy_view.get<TargetDummyComponent>(hit_dummy_entity);
            auto& dummy = dummy_comp.state;

            const float dmg = comp.base_damage;
            const float hs_mult = comp.headshot_multiplier;
            const float raw_damage = is_headshot ? dmg * hs_mult : dmg;

            // Apply damage with armor absorption
            float damage_dealt = raw_damage;
            if (dummy.armor > 0.0F) {
                constexpr float kArmorAbsorption = 0.66F;
                float armor_dmg = raw_damage * kArmorAbsorption;
                if (armor_dmg > dummy.armor) armor_dmg = dummy.armor;
                dummy.armor -= armor_dmg;
                damage_dealt = raw_damage - armor_dmg;
            }
            dummy.health -= damage_dealt;
            dummy.last_hit_timer = 0.3F;
            dummy.was_hit_precision = is_headshot;
            dummy.last_damage_dealt = damage_dealt;
            dummy.last_hit_position = hit_point;

            if (dummy.health <= 0.0F) {
                dummy.health = 0.0F;
                dummy.alive = false;
                dummy.respawn_timer = 3.0F;
                world.on_dummy_killed(dummy.dummy_id, dummy.position);
            }

            // Spawn damage number
            Vec3 num_pos = hit_point;
            num_pos.y += 0.3F;
            world.spawn_damage_number(num_pos, damage_dealt, is_headshot);

            // Hitmarker
            world.set_hitmarker(0.15F, is_headshot);

            // Hit sound
            world.queue_audio_event(AudioEvent{"dummy_hit", 1.0f, AudioCategory::SFX});
        }
    }

    for (auto entity : to_destroy) {
        registry.destroy(entity);
    }
}

// --- fire_hitscan (lag-compensated instant-hit) -------------------------------

void fire_hitscan(World& world, const PlayerInputCommand& input) {
    if (!world.can_fire()) return;

    const auto& def = world.get_active_weapon_def();
    const float base_damage = def.base_damage;
    const float headshot_multiplier = def.headshot_multiplier;

    // Fire cooldown derived from weapon definition RPM.
    world.set_fire_cooldown_timer(def.fire_interval());

    // Shotgun detection by slot (Secondary)
    const bool is_shotgun = (def.slot == WeaponSlot::Secondary);

    world.consume_ammo();
    world.notify_weapon_fired();

    ae::log_info("Fired: " + std::string(weapon_name(world.get_active_weapon_index())) +
                 " | ammo=" + std::to_string(world.get_ammo_current()) +
                 "/" + std::to_string(world.get_ammo_max()));

    const int pellets = is_shotgun ? 8 : 1;
    constexpr float kShotgunSpreadDeg = 5.0F;

    const auto& camera_anchor = world.get_camera_anchor();
    Vec3 origin = camera_anchor.position;

    float yaw_rad = ae::to_radians(camera_anchor.yaw);
    float pitch_rad = ae::to_radians(camera_anchor.pitch);

    for (int p = 0; p < pellets; ++p) {
        Vec3 forward {
            std::sin(yaw_rad) * std::cos(pitch_rad),
            -std::sin(pitch_rad),
            std::cos(yaw_rad) * std::cos(pitch_rad)
        };

        // Shotgun spread — deterministic using recoil index
        if (is_shotgun && p > 0) {
            const int rng_seed = world.recoil_index();
            float spread_yaw = deterministic_spread_offset(rng_seed, p, kShotgunSpreadDeg);
            float spread_pitch = deterministic_spread_offset(rng_seed, p + 1000, kShotgunSpreadDeg);
            float sy = ae::to_radians(camera_anchor.yaw + spread_yaw);
            float sp = ae::to_radians(camera_anchor.pitch + spread_pitch);
            forward = {std::sin(sy) * std::cos(sp), -std::sin(sp), std::cos(sy) * std::cos(sp)};
        }

        constexpr float kHitscanRange = 1000.0F;
        Vec3 ray_end {
            origin.x + forward.x * kHitscanRange,
            origin.y + forward.y * kHitscanRange,
            origin.z + forward.z * kHitscanRange
        };

        // Lag compensation: rewind dummies to historical positions at client tick
        const ae::u32 rewind_tick = input.client_tick;
        ahamkara::game::HistoricalState hist_state = world.get_historical_state(rewind_tick);

        // Find closest dummy hit by ray-vs-capsule test
        float closest_t = kHitscanRange;
    bool hit_something = false;
    bool is_headshot = false;
    int hit_dummy_idx = -1;
    Vec3 hit_position = ray_end;

    auto& registry = world.registry();
    auto dummy_view = registry.view<TargetDummyComponent>();
    int idx = 0;

    for (auto dummy_entity : dummy_view) {
        auto& dummy_comp = dummy_view.get<TargetDummyComponent>(dummy_entity);
        auto& dummy = dummy_comp.state;
        if (!dummy.alive) { ++idx; continue; }

        // Use historical position for lag compensation
        Vec3 d_pos = dummy.position;
        if (idx < hist_state.kMaxDummies && hist_state.tick > 0) {
            d_pos = hist_state.dummy_positions[idx];
        }

        // Dummy capsule: center at position, radius 0.35, half-height 1.0
        float d_radius = 0.35F;
        float d_half_height = 1.0F;
        Vec3 d_bottom = {d_pos.x, d_pos.y - d_half_height, d_pos.z};
        Vec3 d_top = {d_pos.x, d_pos.y + d_half_height, d_pos.z};

        // Ray-vs-capsule (approximate as ray-vs-cylinder + hemisphere caps)
        Vec3 ray_dir = {
            ray_end.x - origin.x,
            ray_end.y - origin.y,
            ray_end.z - origin.z
        };
        float ray_len = std::sqrt(ray_dir.x * ray_dir.x + ray_dir.y * ray_dir.y + ray_dir.z * ray_dir.z);
        if (ray_len < 0.001F) { ++idx; continue; }
        ray_dir.x /= ray_len;
        ray_dir.y /= ray_len;
        ray_dir.z /= ray_len;

        // Test against infinite cylinder first
        Vec3 oc = {origin.x - d_pos.x, origin.y - d_pos.y, origin.z - d_pos.z};
        float a = ray_dir.x * ray_dir.x + ray_dir.z * ray_dir.z;
        float b = 2.0F * (oc.x * ray_dir.x + oc.z * ray_dir.z);
        float c = oc.x * oc.x + oc.z * oc.z - d_radius * d_radius;
        float disc = b * b - 4.0F * a * c;

        if (disc >= 0.0F) {
            float sqrt_disc = std::sqrt(disc);
            float t0 = (-b - sqrt_disc) / (2.0F * a);
            float t1 = (-b + sqrt_disc) / (2.0F * a);
            if (t0 > t1) std::swap(t0, t1);

            // Check cylinder cap (y range)
            for (float t : {t0, t1}) {
                if (t > 0.001F && t < closest_t) {
                    float hit_y = origin.y + ray_dir.y * t;
                    float d_top_y = d_top.y;
                    float d_bot_y = d_bottom.y;
                    if (hit_y >= d_bot_y && hit_y <= d_top_y) {
                        closest_t = t;
                        hit_something = true;
                        hit_dummy_idx = idx;
                        hit_position = {
                            origin.x + ray_dir.x * t,
                            hit_y,
                            origin.z + ray_dir.z * t
                        };
                        is_headshot = (hit_y >= d_top_y - 0.3F);
                    }
                }
            }

            // Hemisphere caps
            Vec3 cap_center = d_top;
            Vec3 to_cap = {origin.x - cap_center.x, origin.y - cap_center.y, origin.z - cap_center.z};
            float cap_b = 2.0F * (to_cap.x * ray_dir.x + to_cap.y * ray_dir.y + to_cap.z * ray_dir.z);
            float cap_c = to_cap.x * to_cap.x + to_cap.y * to_cap.y + to_cap.z * to_cap.z - d_radius * d_radius;
            float cap_disc = cap_b * cap_b - 4.0F * cap_c;

            if (cap_disc >= 0.0F) {
                float cap_t = (-cap_b - std::sqrt(cap_disc)) * 0.5F;
                if (cap_t > 0.001F && cap_t < closest_t) {
                    closest_t = cap_t;
                    hit_something = true;
                    is_headshot = true;
                    hit_dummy_idx = idx;
                    hit_position = {
                        origin.x + ray_dir.x * cap_t,
                        origin.y + ray_dir.y * cap_t,
                        origin.z + ray_dir.z * cap_t
                    };
                }
                // Bottom cap
                cap_center = d_bottom;
                to_cap = {origin.x - cap_center.x, origin.y - cap_center.y, origin.z - cap_center.z};
                cap_b = 2.0F * (to_cap.x * ray_dir.x + to_cap.y * ray_dir.y + to_cap.z * ray_dir.z);
                cap_c = to_cap.x * to_cap.x + to_cap.y * to_cap.y + to_cap.z * to_cap.z - d_radius * d_radius;
                cap_disc = cap_b * cap_b - 4.0F * cap_c;
                if (cap_disc >= 0.0F) {
                    cap_t = (-cap_b - std::sqrt(cap_disc)) * 0.5F;
                    if (cap_t > 0.001F && cap_t < closest_t) {
                        closest_t = cap_t;
                        hit_something = true;
                        is_headshot = false;
                        hit_dummy_idx = idx;
                        hit_position = {
                            origin.x + ray_dir.x * cap_t,
                            origin.y + ray_dir.y * cap_t,
                            origin.z + ray_dir.z * cap_t
                        };
                    }
                }
            }
        }
        ++idx;
    }

    if (hit_something && hit_dummy_idx >= 0) {
        // Apply damage to the hit dummy
        int didx = 0;
        for (auto dummy_entity : dummy_view) {
            if (didx == hit_dummy_idx) {
                auto& dummy_comp = dummy_view.get<TargetDummyComponent>(dummy_entity);
                auto& dummy = dummy_comp.state;

                float raw_damage = is_headshot ? base_damage * headshot_multiplier : base_damage;
                float damage_dealt = raw_damage;
                if (dummy.armor > 0.0F) {
                    constexpr float kArmorAbsorption = 0.66F;
                    float armor_dmg = raw_damage * kArmorAbsorption;
                    if (armor_dmg > dummy.armor) armor_dmg = dummy.armor;
                    dummy.armor -= armor_dmg;
                    damage_dealt = raw_damage - armor_dmg;
                }
                dummy.health -= damage_dealt;
                dummy.last_hit_timer = 0.3F;
                dummy.was_hit_precision = is_headshot;
                dummy.last_damage_dealt = damage_dealt;
                dummy.last_hit_position = hit_position;

                if (dummy.health <= 0.0F) {
                    dummy.health = 0.0F;
                    dummy.alive = false;
                    dummy.respawn_timer = 3.0F;
                    world.on_dummy_killed(dummy.dummy_id, dummy.position);
                }

                Vec3 num_pos = hit_position;
                num_pos.y += 0.3F;
                world.spawn_damage_number(num_pos, damage_dealt, is_headshot);
                world.set_hitmarker(0.15F, is_headshot);
                world.queue_audio_event(AudioEvent{"dummy_hit", 1.0f, AudioCategory::SFX});

                Vec3 normal = {-forward.x, -forward.y, -forward.z};
                world.spawn_impact_particles(hit_position, normal);
                world.spawn_bullet_hole_decal(hit_position, normal);
                break;
            }
            ++didx;
        }
    }
    }  // end pellet loop

    // Muzzle flash (outside pellet loop)
    world.set_muzzle_flash(0.05F);
    const auto& orig_anchor = world.get_camera_anchor();
    float oy = ae::to_radians(orig_anchor.yaw);
    float op = ae::to_radians(orig_anchor.pitch);
    Vec3 origin_fx = orig_anchor.position;
    Vec3 fwd_fx {std::sin(oy) * std::cos(op), -std::sin(op), std::cos(oy) * std::cos(op)};
    world.spawn_muzzle_particles(origin_fx, fwd_fx);

    // Apply deterministic recoil from weapon definition pattern
    apply_recoil(world, def);
}

}  // namespace ahamkara::game
