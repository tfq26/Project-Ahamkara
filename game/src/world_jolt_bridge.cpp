#include "world_jolt_bridge.h"
#include "ahamkara/game/world.h"

// Access the engine-internal Jolt backend for PhysicsSystem access
#include "jolt_backend.h"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

namespace ahamkara::game {

// ============================================================
// JoltWorldImpl constructor / destructor
// ============================================================

JoltWorldImpl::JoltWorldImpl(World* world)
    : collision_world() // engine-owned world (initializes Jolt once)
      ,
      contact_listener(world, &collision_world) {
    // Character is created by world.cpp after this constructor runs.
    // All Jolt initialization is handled by CollisionWorld::Impl.
}

JPH::PhysicsSystem& JoltWorldImpl::get_physics_system() {
    return *collision_world.impl()->physics();
}

JoltWorldImpl::~JoltWorldImpl() {
    auto* phys = collision_world.impl()->physics();
    auto& bi = phys->GetBodyInterface();
    for (auto id : map_bodies) {
        bi.RemoveBody(id);
        bi.DestroyBody(id);
    }
    for (auto id : dummy_bodies) {
        bi.RemoveBody(id);
        bi.DestroyBody(id);
    }
    // character is destroyed by unique_ptr, collision_world by its own destructor
}

// ============================================================
// Character contact validation
// ============================================================

bool AhamkaraCharacterContactListener::OnContactValidate(
    const JPH::CharacterVirtual* inCharacter,
    const JPH::BodyID& inBodyID2,
    const JPH::SubShapeID& /*inSubShapeID2*/) {
    if (!world_ || !world_->jolt_ || !world_->colliders_) {
        return true;
    }

    auto& bi = collision_world_->impl()->physics()->GetBodyInterface();
    JPH::uint64 userData = bi.GetUserData(inBodyID2);

    if (userData < world_->collider_count_) {
        const auto& c = world_->colliders_[userData];
        if (c.jump_through) {
            // Reject collision if character is moving upwards
            float vy = inCharacter->GetLinearVelocity().GetY();
            if (vy > 0.01F) {
                return false;
            }
        }
    }

    return true;
}

// ============================================================
// Collider recreation
// ============================================================

void rebuild_jolt_colliders(
    JoltWorldImpl& jolt,
    const ColliderBox* colliders,
    std::size_t collider_count,
    const TargetDummyState* dummies,
    int dummy_count,
    const JPH::RefConst<JPH::Shape>& /*standing_shape*/) {

    auto* phys = jolt.collision_world.impl()->physics();
    auto& bi = phys->GetBodyInterface();

    // Remove existing map bodies
    for (auto id : jolt.map_bodies) {
        bi.RemoveBody(id);
        bi.DestroyBody(id);
    }
    jolt.map_bodies.clear();

    // Remove existing dummy bodies
    for (auto id : jolt.dummy_bodies) {
        bi.RemoveBody(id);
        bi.DestroyBody(id);
    }
    jolt.dummy_bodies.clear();

    // Create map collider bodies using engine-defined layers
    for (std::size_t i = 0; i < collider_count; ++i) {
        const auto& c = colliders[i];
        float hx = (c.max_x - c.min_x) * 0.5f;
        float hz = (c.max_z - c.min_z) * 0.5f;
        float hy = (c.top_y - c.bottom_y) * 0.5f;
        float cx = (c.min_x + c.max_x) * 0.5f;
        float cz = (c.min_z + c.max_z) * 0.5f;
        float cy = (c.bottom_y + c.top_y) * 0.5f;

        if (hx < 0.01f && hz < 0.01f) {
            continue; // Skip degenerates
        }

        JPH::BoxShapeSettings shape_settings(JPH::Vec3(hx, hy, hz));
        shape_settings.SetEmbedded();
        JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();
        if (!shape_result.IsValid()) continue;

        JPH::BodyCreationSettings body_settings(
            shape_result.Get(),
            JPH::RVec3(cx, cy, cz),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Static,
            ae::collision::jolt_helpers::kJoltLayerStatic // unified: uses engine layer defs
        );
        body_settings.mUserData = static_cast<JPH::uint64>(i);

        JPH::BodyID body_id = bi.CreateAndAddBody(body_settings, JPH::EActivation::DontActivate);
        jolt.map_bodies.push_back(body_id);
    }

    // Create dummy bodies (kinematic capsules) using engine-defined layers
    for (int i = 0; i < dummy_count; ++i) {
        const auto& d = dummies[i];
        if (!d.alive) continue;

        JPH::CapsuleShapeSettings capsule_settings(1.0f, 0.35f);
        capsule_settings.SetEmbedded();
        JPH::ShapeSettings::ShapeResult shape_result = capsule_settings.Create();
        if (!shape_result.IsValid()) continue;

        JPH::BodyCreationSettings body_settings(
            shape_result.Get(),
            JPH::RVec3(d.position.x, d.position.y, d.position.z),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Kinematic,
            ae::collision::jolt_helpers::kJoltLayerNpc // unified: uses engine layer defs
        );

        JPH::BodyID body_id = bi.CreateAndAddBody(body_settings, JPH::EActivation::Activate);
        jolt.dummy_bodies.push_back(body_id);
    }
}

}  // namespace ahamkara::game
