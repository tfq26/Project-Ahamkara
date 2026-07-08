#pragma once

#include "ahamkara/game/net_types.h"
#include "ahamkara/game/world.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

namespace ahamkara::game {

struct WorldProjectileComponent {
    ProjectileState state;
    float base_damage {25.0F};
    float headshot_multiplier {2.0F};
    ae::u32 owner_id {0};
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
void fire_hitscan(World& world, const PlayerInputCommand& input);
void step_projectiles(World& world, float delta_seconds);

}  // namespace ahamkara::game
