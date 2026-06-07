#include "world_jolt_bridge.h"
#include "ahamkara/game/world.h"

#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

namespace ahamkara::game {

// --- One-time Jolt initialization -------------------------------------------

void initialize_jolt_once() {
    static bool initialized = false;
    if (!initialized) {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
        initialized = true;
    }
}

// --- Character contact validation --------------------------------------------

bool AhamkaraCharacterContactListener::OnContactValidate(
    const JPH::CharacterVirtual* /*inCharacter*/,
    const JPH::BodyID& /*inBodyID2*/,
    const JPH::SubShapeID& /*inSubShapeID2*/) {
    // Always allow contacts; additional game-specific filtering can be added here.
    return true;
}

// --- Collider recreation ------------------------------------------------------

void rebuild_jolt_colliders(
    JoltWorldImpl& jolt,
    const ColliderBox* colliders,
    std::size_t collider_count,
    const TargetDummyState* dummies,
    int dummy_count,
    const JPH::RefConst<JPH::Shape>& /*standing_shape*/) {

    auto& bi = jolt.physics_system.GetBodyInterface();

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

    // Create map collider bodies
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
            c.wall ? JPH::EMotionType::Static : JPH::EMotionType::Static,
            Layers::NON_MOVING
        );
        body_settings.mUserData = static_cast<JPH::uint64>(i);

        JPH::BodyID body_id = bi.CreateAndAddBody(body_settings, JPH::EActivation::DontActivate);
        jolt.map_bodies.push_back(body_id);
    }

    // Create dummy bodies (kinematic capsules)
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
            Layers::MOVING
        );

        JPH::BodyID body_id = bi.CreateAndAddBody(body_settings, JPH::EActivation::Activate);
        jolt.dummy_bodies.push_back(body_id);
    }
}

}  // namespace ahamkara::game
