#pragma once

#include "ahamkara/game/world.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <entt/entt.hpp>
#include <vector>

namespace ahamkara::game {

struct TargetDummyComponent {
    TargetDummyState state;
};

void tick_dummies(
    entt::registry& registry,
    TargetDummyState* dummies,
    int dummy_count,
    ae::u32 current_tick,
    float delta_seconds);

void sync_dummies_to_jolt(
    JPH::PhysicsSystem& physics_system,
    std::vector<JPH::BodyID>& dummy_bodies,
    const TargetDummyState* dummies,
    int dummy_count);

}  // namespace ahamkara::game
