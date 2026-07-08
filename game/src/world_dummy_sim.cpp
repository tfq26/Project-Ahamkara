#include "world_dummy_sim.h"
#include "world_jolt_bridge.h"

#include "ahamkara/game/movement.h"
#include "ae/core/math.h"

#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include <cmath>
#include <algorithm>
#include <cstdlib>

namespace ahamkara::game {

namespace {

constexpr float kDummyWeaponDamage = 8.0F;
constexpr float kDummyMaxEngageDistance = 60.0F;
constexpr float kDummyIdealEngageDistance = 15.0F;
constexpr float kDummyAimSpeed = 180.0F;
constexpr float kDummyFireInterval = 0.8F;
constexpr float kDummyBurstInterval = 0.12F;
constexpr int kDummyBurstCount = 4;
constexpr float kDummyInaccuracyDeg = 6.0F;

float angle_difference_deg(float a, float b) {
    float diff = std::fmod(a - b + 540.0F, 360.0F);
    if (diff > 180.0F) diff -= 360.0F;
    return diff;
}

bool line_of_sight_clear(
    const Vec3& from,
    const Vec3& to,
    const std::vector<ColliderBox>& colliders,
    const Vec3& player_pos) {
    // Quick check: if distance is very small, LOS is clear
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float dz = to.z - from.z;
    float dist = std::sqrt(dx * dx + dz * dz);
    if (dist < 0.5F) return true;

    // Check against each collider (2D wall check)
    for (const auto& c : colliders) {
        if (!c.wall) continue;
        float min_x = std::min(c.min_x, c.max_x);
        float max_x = std::max(c.min_x, c.max_x);
        float min_z = std::min(c.min_z, c.max_z);
        float max_z = std::max(c.min_z, c.max_z);

        float top_y = std::max(c.top_y, c.bottom_y);
        float bottom_y = std::min(c.top_y, c.bottom_y);

        if (top_y < from.y && top_y < to.y) continue;
        if (bottom_y > from.y + 2.0F && bottom_y > to.y + 2.0F) continue;

        float dx_line = to.x - from.x;
        float dz_line = to.z - from.z;
        float len = std::sqrt(dx_line * dx_line + dz_line * dz_line);
        if (len < 0.0001F) continue;
        float nx = -dz_line / len;
        float nz = dx_line / len;

        float proj_min = nx * (min_x - from.x) + nz * (min_z - from.z);
        float proj_max = nx * (max_x - from.x) + nz * (max_z - from.z);
        float proj_cx = nx * (min_x + max_x) * 0.5F - nx * from.x + nz * (min_z + max_z) * 0.5F - nz * from.z;
        float half_extent = std::abs(proj_max - proj_min) * 0.5F;

        if (std::abs(proj_cx) < half_extent + 0.3F) {
            float t = ((min_x + max_x) * 0.5F - from.x) * dx_line + ((min_z + max_z) * 0.5F - from.z) * dz_line;
            t /= (dx_line * dx_line + dz_line * dz_line);
            if (t > 0.01F && t < 0.99F) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace

void tick_dummies(
    entt::registry& registry,
    float delta_seconds) {

    auto view = registry.view<TargetDummyComponent>();
    for (auto entity : view) {
        auto& comp = view.get<TargetDummyComponent>(entity);
        auto& d = comp.state;

        // Respawn timer
        if (!d.alive) {
            d.respawn_timer -= delta_seconds;
            if (d.respawn_timer <= 0.0F) {
                d.alive = true;
                d.health = 100.0F;
                d.position = d.start_position;
                d.last_hit_timer = 0.0F;
                comp.fire_timer = 1.0F;
                comp.burst_timer = 0.0F;
                comp.burst_count = 0;
            }
            continue;
        }

        // Hit feedback timer
        if (d.last_hit_timer > 0.0F) {
            d.last_hit_timer = std::max(0.0F, d.last_hit_timer - delta_seconds);
        }

        // Movement: oscillate along move_dir
        if (d.move_speed > 0.0F && d.move_distance > 0.0F) {
            d.move_timer += delta_seconds * d.move_speed;
            float offset = std::sin(d.move_timer) * d.move_distance;
            d.position.x = d.start_position.x + d.move_dir.x * offset;
            d.position.z = d.start_position.z + d.move_dir.z * offset;
        }
    }
}

void tick_dummy_ai(
    entt::registry& registry,
    float delta_seconds,
    const Vec3& player_position,
    const std::vector<ColliderBox>& world_colliders,
    World& world) {

    auto view = registry.view<TargetDummyComponent>();
    for (auto entity : view) {
        auto& comp = view.get<TargetDummyComponent>(entity);
        auto& d = comp.state;
        if (!d.alive) continue;

        // Direction to player
        float dx = player_position.x - d.position.x;
        float dz = player_position.z - d.position.z;
        float dist = std::sqrt(dx * dx + dz * dz);
        if (dist < 0.1F) continue;

        // Target yaw toward player
        float target_yaw = std::atan2(dx, dz) * (180.0F / 3.1415926535F);

        // Smoothly rotate toward player
        float yaw_diff = angle_difference_deg(target_yaw, d.yaw);
        float max_rotation = kDummyAimSpeed * delta_seconds;
        if (std::abs(yaw_diff) < max_rotation) {
            d.yaw = target_yaw;
        } else {
            d.yaw += (yaw_diff > 0 ? 1.0F : -1.0F) * max_rotation;
        }
        d.yaw = std::fmod(d.yaw + 360.0F, 360.0F);

        // Only engage within range
        if (dist > kDummyMaxEngageDistance) continue;

        // Check line of sight
        Vec3 eye_pos {d.position.x, d.position.y + 1.0F, d.position.z};
        Vec3 player_center {player_position.x, player_position.y + 0.9F, player_position.z};
        if (!line_of_sight_clear(eye_pos, player_center, world_colliders, player_position)) continue;

        // Fire timer
        comp.fire_timer -= delta_seconds;

        // Handle burst firing
        if (comp.burst_count > 0) {
            comp.burst_timer -= delta_seconds;
            if (comp.burst_timer <= 0.0F) {
                comp.burst_timer = kDummyBurstInterval;
                comp.burst_count--;
                // Fire a shot
                float inaccuracy = (static_cast<float>(std::rand() % 1000) / 1000.0F - 0.5F) * kDummyInaccuracyDeg;
                float fire_yaw = d.yaw + inaccuracy;
                float fire_pitch = (static_cast<float>(std::rand() % 1000) / 1000.0F - 0.5F) * 3.0F;

                // Apply damage to player (hitscan)
                float damage = kDummyWeaponDamage;
                world.apply_damage_to_player(damage, d.position);
                world.queue_audio_event(AudioEvent{"dummy_fire", 0.7f, AudioCategory::Weapon});

                if (comp.burst_count <= 0) {
                    comp.fire_timer = kDummyFireInterval + (static_cast<float>(std::rand() % 100) / 100.0F) * 0.5F;
                }
            }
        } else if (comp.fire_timer <= 0.0F) {
            // Start a burst
            comp.burst_count = kDummyBurstCount;
            comp.burst_timer = 0.0F;
        }
    }
}

void sync_dummies_to_jolt(
    JPH::PhysicsSystem& physics_system,
    std::vector<JPH::BodyID>& dummy_bodies,
    const entt::registry& registry) {

    auto& bi = physics_system.GetBodyInterface();
    auto view = registry.view<TargetDummyComponent>();
    int dummy_count = 0;
    for (auto entity : view) {
        (void)entity;
        dummy_count++;
    }

    // Ensure we have enough bodies
    while ((int)dummy_bodies.size() < dummy_count) {
        JPH::CapsuleShapeSettings capsule_settings(1.0f, 0.35f);
        capsule_settings.SetEmbedded();
        JPH::ShapeSettings::ShapeResult shape_result = capsule_settings.Create();
        if (!shape_result.IsValid()) break;

        JPH::BodyCreationSettings body_settings(
            shape_result.Get(),
            JPH::RVec3(0.0f, 0.0f, 0.0f),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Kinematic,
            Layers::MOVING
        );

        JPH::BodyID body_id = bi.CreateAndAddBody(body_settings, JPH::EActivation::Activate);
        dummy_bodies.push_back(body_id);
    }

    int i = 0;
    for (auto entity : view) {
        if (i >= (int)dummy_bodies.size()) break;
        const auto& comp = view.get<TargetDummyComponent>(entity);
        const auto& d = comp.state;
        if (!d.alive) {
            // Move dead dummies far below the map
            bi.SetPosition(dummy_bodies[i], JPH::RVec3(0.0f, -100.0f, 0.0f), JPH::EActivation::DontActivate);
        } else {
            bi.SetPosition(dummy_bodies[i],
                JPH::RVec3(d.position.x, d.position.y, d.position.z),
                JPH::EActivation::DontActivate);
        }
        i++;
    }
}

}  // namespace ahamkara::game
