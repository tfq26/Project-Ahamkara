#pragma once
// Internal header — do not include from game code.
// Defines the Jolt-backed CollisionWorld::Impl for use by
// collision_world.cpp and trace.cpp.

#include "ae/collision/world.h"
#include "ae/core/log.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

#include <unordered_map>

namespace ae::collision {

// Internal body record
struct InternalBody {
    JPH::BodyID jolt_id;
    BodyDef def;
    bool active {true};
};

// Jolt layer mapping helpers
namespace jolt_helpers {

    constexpr JPH::ObjectLayer kJoltLayerStatic    = 0;
    constexpr JPH::ObjectLayer kJoltLayerDynamic   = 1;
    constexpr JPH::ObjectLayer kJoltLayerPlayer    = 2;
    constexpr JPH::ObjectLayer kJoltLayerNpc       = 3;
    constexpr JPH::ObjectLayer kJoltLayerProjectile = 4;
    constexpr JPH::ObjectLayer kJoltLayerTrigger   = 5;
    constexpr JPH::ObjectLayer kJoltLayerPickup    = 6;
    constexpr JPH::ObjectLayer kJoltLayerVehicle   = 7;
    constexpr JPH::ObjectLayer kJoltLayerDebris    = 8;
    constexpr JPH::ObjectLayer kJoltLayerCamera    = 9;

    constexpr JPH::ObjectLayer kJoltNumLayers = 16;

    inline     JPH::ObjectLayer to_jolt_layer(CollisionLayer layer) {
        if (layer <= 9) return static_cast<JPH::ObjectLayer>(layer);
        if (layer >= GameLayers::GAME_LAYER_FIRST) {
            u32 offset = layer - GameLayers::GAME_LAYER_FIRST;
            return static_cast<JPH::ObjectLayer>(10 + offset);
        }
        ae::log_warning_cat("Collision", "to_jolt_layer: unexpected layer " + std::to_string(static_cast<int>(layer)) +
                            " — falling back to Static");
        return kJoltLayerStatic;
    }

    namespace BPLayers {
        static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        static constexpr JPH::BroadPhaseLayer MOVING(1);
        static constexpr JPH::uint NUM_LAYERS(2);
    }
}  // namespace jolt_helpers

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        for (int i = 0; i < jolt_helpers::kJoltNumLayers; ++i) {
            mObjectToBroadPhase[i] = (i == jolt_helpers::kJoltLayerStatic)
                ? jolt_helpers::BPLayers::NON_MOVING
                : jolt_helpers::BPLayers::MOVING;
        }
    }

    JPH::uint GetNumBroadPhaseLayers() const override { return jolt_helpers::BPLayers::NUM_LAYERS; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        if (inLayer < jolt_helpers::kJoltNumLayers) return mObjectToBroadPhase[inLayer];
        return jolt_helpers::BPLayers::MOVING;
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        switch ((JPH::BroadPhaseLayer::Type)inLayer) {
            case (JPH::BroadPhaseLayer::Type)jolt_helpers::BPLayers::NON_MOVING: return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type)jolt_helpers::BPLayers::MOVING:     return "MOVING";
            default:                                                               return "UNKNOWN";
        }
    }
#endif

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[jolt_helpers::kJoltNumLayers];
};

class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
        if (inObject1 == jolt_helpers::kJoltLayerStatic && inObject2 == jolt_helpers::kJoltLayerStatic) return false;
        if (inObject1 == jolt_helpers::kJoltLayerDebris && inObject2 == jolt_helpers::kJoltLayerDebris) return false;
        return true;
    }
};

class ObjectVsBPLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        if (inLayer1 == jolt_helpers::kJoltLayerStatic) return inLayer2 == jolt_helpers::BPLayers::MOVING;
        return true;
    }
};

// All-pass filters (for trace queries that filter via BodyFilter instead)
class AllPassBroadPhaseLayerFilter : public JPH::BroadPhaseLayerFilter {
public:
    bool ShouldCollide([[maybe_unused]] JPH::BroadPhaseLayer) const override { return true; }
};

class AllPassObjectLayerFilter : public JPH::ObjectLayerFilter {
public:
    bool ShouldCollide([[maybe_unused]] JPH::ObjectLayer) const override { return true; }
};

// ============================================================
// CollisionWorld::Impl
// ============================================================
class CollisionWorld::Impl {
public:
    JPH::JobSystemThreadPool job_system;
    JPH::PhysicsSystem physics_system;
    ObjectVsBPLayerFilterImpl ob_bp_filter;
    ObjectLayerPairFilterImpl ob_ob_filter;
    JPH::TempAllocatorImpl temp_allocator;
    BPLayerInterfaceImpl bp_layer_interface;

    // Controlled accessor for character controller / bridge code.
    // Not part of the public CollisionWorld API — intended for
    // engine-internal consumers (character.cpp, world_jolt_bridge.cpp).
    [[nodiscard]] JPH::PhysicsSystem* physics() { return &physics_system; }
    [[nodiscard]] const JPH::PhysicsSystem* physics() const { return &physics_system; }
    [[nodiscard]] JPH::TempAllocator* temp_allocator_ptr() { return &temp_allocator; }

    std::unordered_map<BodyHandle, InternalBody> bodies;
    BodyHandle next_handle {1};

    Impl()
        : Impl(ensure_jolt_initialized()) {}

    ~Impl() {
        std::size_t body_count = bodies.size();
        auto& bi = physics_system.GetBodyInterface();
        for (auto& [handle, body] : bodies) {
            if (!body.jolt_id.IsInvalid()) {
                bi.RemoveBody(body.jolt_id);
                bi.DestroyBody(body.jolt_id);
            }
        }
        bodies.clear();
        ae::log_info_cat("Collision", "Jolt Physics system shut down (" +
                         std::to_string(body_count) + " bodies destroyed)");
    }

    [[nodiscard]] JPH::ShapeRefC create_shape(const ColliderDef& def) {
        JPH::ShapeSettings::ShapeResult result;

        switch (def.shape) {
            case ColliderShape::Box: {
                JPH::BoxShapeSettings settings(
                    JPH::Vec3(def.half_extents.x, def.half_extents.y, def.half_extents.z)
                );
                result = settings.Create();
                break;
            }
            case ColliderShape::Sphere: {
                JPH::SphereShapeSettings settings(def.radius);
                result = settings.Create();
                break;
            }
            case ColliderShape::Capsule: {
                JPH::CapsuleShapeSettings settings(def.capsule_half_height, def.radius);
                result = settings.Create();
                break;
            }
            case ColliderShape::TriangleMesh: {
                if (def.triangles && def.triangle_count > 0) {
                    JPH::VertexList vertices;
                    JPH::IndexedTriangleList indices;
                    vertices.reserve(static_cast<size_t>(def.triangle_count) * 3);
                    indices.reserve(static_cast<size_t>(def.triangle_count));

                    for (int i = 0; i < def.triangle_count; ++i) {
                        const auto& t = def.triangles[i];
                        JPH::Float3 v0{t.v0.x, t.v0.y, t.v0.z};
                        JPH::Float3 v1{t.v1.x, t.v1.y, t.v1.z};
                        JPH::Float3 v2{t.v2.x, t.v2.y, t.v2.z};
                        vertices.push_back(v0);
                        vertices.push_back(v1);
                        vertices.push_back(v2);
                        indices.push_back(JPH::IndexedTriangle(
                            static_cast<JPH::uint32>(i * 3),
                            static_cast<JPH::uint32>(i * 3 + 1),
                            static_cast<JPH::uint32>(i * 3 + 2),
                            0
                        ));
                    }

                    JPH::MeshShapeSettings settings(std::move(vertices), std::move(indices));
                    result = settings.Create();
                } else {
                    ae::log_warning_cat("Collision",
                                        "create_shape: TriangleMesh has 0 triangles; falling back to 0.01 unit box");
                    JPH::BoxShapeSettings settings(JPH::Vec3(0.01F, 0.01F, 0.01F));
                    result = settings.Create();
                }
                break;
            }
            case ColliderShape::ConvexHull:
            default: {
                JPH::BoxShapeSettings settings(
                    JPH::Vec3(def.half_extents.x, def.half_extents.y, def.half_extents.z)
                );
                result = settings.Create();
                break;
            }
        }

        if (result.HasError()) {
            ae::log_warning_cat("Collision", "create_shape: shape creation failed — " +
                                std::string(result.GetError().c_str()));
            return nullptr;
        }
        ae::log_debug_cat("Collision", "create_shape: created " +
                          std::to_string(static_cast<int>(def.shape)) + " shape");
        return result.Get();
    }

private:
    // Force Jolt initialization before member construction.
    // The static JoltInitToken guarantees RegisterDefaultAllocator
    // runs before temp_allocator/job_system are constructed.
    struct JoltInitToken { explicit JoltInitToken() {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
        ae::log_info_cat("Collision", "Jolt Physics: registered default allocator, factory, and types");
    }};
    static const JoltInitToken& ensure_jolt_initialized() {
        static JoltInitToken token;
        return token;
    }

    Impl(const JoltInitToken&)
        : temp_allocator(10 * 1024 * 1024)
        , job_system(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, 1)
    {
        physics_system.Init(
            2048,    // max bodies
            0,       // mutex count
            2048,    // max body pairs
            4096,    // max contact constraints
            bp_layer_interface,
            ob_bp_filter,
            ob_ob_filter
        );
        ae::log_info_cat("Collision", "Jolt Physics system initialized: max_bodies=2048, "
                         "max_pairs=2048, max_contacts=4096, temp_allocator=10MB, worker_threads=1");
    }
};

// Vector conversion helpers (used by both .cpp files)
inline JPH::RVec3 to_jolt_rvec3(const Vec3& v) {
    return JPH::RVec3(v.x, v.y, v.z);
}

inline JPH::Vec3 to_jolt_vec3(const Vec3& v) {
    return JPH::Vec3(v.x, v.y, v.z);
}

inline Vec3 from_jolt_rvec3(const JPH::RVec3& v) {
    return {v.GetX(), v.GetY(), v.GetZ()};
}

inline Vec3 from_jolt_vec3(const JPH::Vec3& v) {
    return {v.GetX(), v.GetY(), v.GetZ()};
}

}  // namespace ae::collision
