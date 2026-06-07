#pragma once

#include "ahamkara/game/net_types.h"
#include "ahamkara/game/world.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

namespace ahamkara::game {

struct ProjectileComponent {
    ProjectileState state;
};

class BroadPhaseLayerFilterAll : public JPH::BroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(JPH::BroadPhaseLayer /*inLayer*/) const override {
        return true;
    }
};

class ObjectLayerFilterAll : public JPH::ObjectLayerFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer /*inLayer*/) const override {
        return true;
    }
};

void fire_projectile(World& world, const PlayerInputCommand& input);
void step_projectiles(World& world, float delta_seconds);

}  // namespace ahamkara::game
