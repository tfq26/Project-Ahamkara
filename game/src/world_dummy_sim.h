#pragma once

#include "ahamkara/game/net_types.h"
#include "ahamkara/game/world.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

#include <entt/entt.hpp>
#include <vector>

namespace ae::collision { class CollisionWorld; }

namespace ahamkara::game {

struct TargetDummyComponent {
    TargetDummyState state;
    float fire_timer {0.0F};
    float fire_interval {0.8F};
    float aim_yaw {0.0F};
    float aim_pitch {0.0F};
    float burst_timer {0.0F};
    int burst_count {0};
    static constexpr int kMaxBurstShots = 4;
    static constexpr float kBurstInterval = 0.12F;
};

void tick_dummies(
    entt::registry& registry,
    float delta_seconds);

void sync_dummies_to_jolt(
    ae::collision::CollisionWorld& collision_world,
    std::vector<JPH::BodyID>& dummy_bodies,
    const entt::registry& registry);

void tick_dummy_ai(
    entt::registry& registry,
    float delta_seconds,
    const Vec3& player_position,
    const std::vector<ColliderBox>& world_colliders,
    World& world);

}  // namespace ahamkara::game
