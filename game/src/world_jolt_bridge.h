#pragma once
// Private header — game/src/ only.
// Bridges the engine-owned CollisionWorld + CharacterController to game code.
// Game public headers must NOT include this file or any Jolt headers directly.

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>

#include "ae/collision/world.h"
#include "ae/collision/character.h"
#include "ahamkara/game/debug_map.h"

#include <memory>
#include <vector>

namespace ahamkara::game {

struct TargetDummyState;
class World;

// --- Character Contact Listener ------------------------------------------------

class AhamkaraCharacterContactListener : public JPH::CharacterContactListener {
private:
    World* world_;
    ae::collision::CollisionWorld* collision_world_;
public:
    AhamkaraCharacterContactListener(World* world, ae::collision::CollisionWorld* cw)
        : world_(world), collision_world_(cw) {}

    bool OnContactValidate(const JPH::CharacterVirtual* inCharacter,
                          const JPH::BodyID& inBodyID2,
                          const JPH::SubShapeID& inSubShapeID2) override;
};

// --- JoltWorldImpl (GamePhysics) ------------------------------------------------

struct JoltWorldImpl {
    // Engine-owned world — the single authoritative physics simulation
    ae::collision::CollisionWorld collision_world;

    // Character controller — wraps JPH::CharacterVirtual
    std::unique_ptr<ae::collision::CharacterController> character;

    // Cached shapes for crouch/stand transitions (opaque handles)
    JPH::RefConst<JPH::Shape> standing_shape;
    JPH::RefConst<JPH::Shape> crouching_shape;
    bool is_crouched {false};   // Tracks last set shape

    // Bridge body storage — raw Jolt BodyIDs managed by game code.
    // Will be migrated to CollisionWorld::BodyHandle in a later phase.
    std::vector<JPH::BodyID> map_bodies;
    std::vector<JPH::BodyID> dummy_bodies;

    AhamkaraCharacterContactListener contact_listener;

    JoltWorldImpl(World* world);
    ~JoltWorldImpl();

    // Temporary: exposes the engine-owned PhysicsSystem for body bridge
    // code that has not yet migrated to BodyHandle.
    JPH::PhysicsSystem& get_physics_system();
};

// Alias kept for backward compat with world.h forward declaration
using GamePhysics = JoltWorldImpl;

// --- Collider recreation helper -------------------------------------------------

void rebuild_jolt_colliders(
    JoltWorldImpl& jolt,
    const ColliderBox* colliders,
    std::size_t collider_count,
    const TargetDummyState* dummies,
    int dummy_count,
    const JPH::RefConst<JPH::Shape>& standing_shape);

}  // namespace ahamkara::game
