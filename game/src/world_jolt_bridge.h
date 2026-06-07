#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include "ahamkara/game/debug_map.h"

#include <vector>

namespace ahamkara::game {

// --- Jolt Physics Layers ------------------------------------------------------

namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
}

namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr JPH::uint NUM_LAYERS(2);
}

// --- Jolt Filter Implementations ----------------------------------------------

class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
        switch (inObject1) {
            case Layers::NON_MOVING:
                return inObject2 == Layers::MOVING;
            case Layers::MOVING:
                return true;
            default:
                return false;
        }
    }
};

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    virtual JPH::uint GetNumBroadPhaseLayers() const override {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        switch ((JPH::BroadPhaseLayer::Type)inLayer) {
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:     return "MOVING";
            default:                                                       return "INVALID";
        }
    }
#endif

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
            case Layers::NON_MOVING:
                return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:
                return true;
            default:
                return false;
        }
    }
};

// --- Character Contact Listener ------------------------------------------------

class World;

class AhamkaraCharacterContactListener : public JPH::CharacterContactListener {
private:
    World* world_;
public:
    AhamkaraCharacterContactListener(World* world) : world_(world) {}

    virtual bool OnContactValidate(const JPH::CharacterVirtual* inCharacter, const JPH::BodyID& inBodyID2, const JPH::SubShapeID& inSubShapeID2) override;
};

// --- JoltWorldImpl -------------------------------------------------------------

struct TargetDummyState;

struct JoltWorldImpl {
    JPH::TempAllocatorImpl temp_allocator;
    JPH::JobSystemThreadPool job_system;

    BPLayerInterfaceImpl bp_layer_interface;
    ObjectVsBroadPhaseLayerFilterImpl ob_bp_filter;
    ObjectLayerPairFilterImpl ob_ob_filter;

    JPH::PhysicsSystem physics_system;

    JPH::RefConst<JPH::Shape> standing_shape;
    JPH::RefConst<JPH::Shape> crouching_shape;

    JPH::CharacterVirtual* character = nullptr;
    std::vector<JPH::BodyID> map_bodies;
    std::vector<JPH::BodyID> dummy_bodies;

    AhamkaraCharacterContactListener contact_listener;

    JoltWorldImpl(World* world)
        : temp_allocator(10 * 1024 * 1024)
        , job_system(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, 1)
        , contact_listener(world) {

        physics_system.Init(
            1024,
            0,
            1024,
            1024,
            bp_layer_interface,
            ob_bp_filter,
            ob_ob_filter
        );
    }

    ~JoltWorldImpl() {
        if (character) {
            delete character;
        }
        auto& bi = physics_system.GetBodyInterface();
        for (auto id : map_bodies) {
            bi.RemoveBody(id);
            bi.DestroyBody(id);
        }
        for (auto id : dummy_bodies) {
            bi.RemoveBody(id);
            bi.DestroyBody(id);
        }
    }
};

// Alias used by world.cpp
using GamePhysics = JoltWorldImpl;

// --- Initialization ------------------------------------------------------------

void initialize_jolt_once();

// --- Collider recreation helper ------------------------------------------------

void rebuild_jolt_colliders(
    JoltWorldImpl& jolt,
    const ColliderBox* colliders,
    std::size_t collider_count,
    const TargetDummyState* dummies,
    int dummy_count,
    const JPH::RefConst<JPH::Shape>& standing_shape);

}  // namespace ahamkara::game
