#pragma once

#include "ae/collision/types.h"
#include "ae/collision/layers.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace ae::collision {
class CharacterController;
struct CharacterDef;
} // namespace ae::collision

namespace ae::collision {

// ============================================================
// CollisionWorld — the central collision simulation container
//
// This is the interface between game code and the physics backend
// (Jolt Physics). It holds all collision bodies (static and dynamic),
// runs the broadphase/narrowphase, and services trace queries.
//
// The implementation is in engine/collision/src/collision_world.cpp.
// Game code sees only this header; the Jolt headers are not leaked.
// ============================================================

/** Description for creating a static or dynamic rigid body. */
enum class BodyType : u8 {
    Static,              ///< Never moves (world geometry)
    Kinematic,           ///< Moves under game control, not simulated physics
    Dynamic,             ///< Fully physics-simulated
};

enum class ColliderShape : u8 {
    Box,
    Sphere,
    Capsule,
    TriangleMesh,
    ConvexHull,
};

/** Defines the collision geometry for a body. */
struct ColliderDef {
    ColliderShape shape {ColliderShape::Box};

    // Box parameters
    Vec3 half_extents {0.5F, 0.5F, 0.5F};

    // Sphere/Capsule parameters
    float radius {0.5F};

    // Capsule parameters
    float capsule_half_height {0.5F};

    // Triangle mesh parameters
    const Triangle* triangles {nullptr};
    int triangle_count {0};
};

/** Description for creating a collision body. */
struct BodyDef {
    BodyType type {BodyType::Static};
    ColliderDef collider {};
    Vec3 position {};
    CollisionLayer layer {GameLayers::WORLD_STATIC};
    CollisionMask collision_mask {};   ///< Which layers this body collides with
    u64 user_data {0};                 ///< Application-defined data
    bool is_sensor {false};            ///< True = trigger volume (no physical collision)
};

/** Opaque handle to a body in the collision world. */
using BodyHandle = u32;
constexpr BodyHandle kInvalidBodyHandle = 0xFFFFFFFF;

/** Statistics about the collision world for debugging. */
struct CollisionStats {
    int body_count {0};
    int active_dynamic_bodies {0};
    int broadphase_pairs {0};
    int narrowphase_contacts {0};
    int trigger_events {0};
};

class CollisionWorld {
public:
    CollisionWorld();
    ~CollisionWorld();

    CollisionWorld(const CollisionWorld&) = delete;
    CollisionWorld& operator=(const CollisionWorld&) = delete;
    CollisionWorld(CollisionWorld&&) = delete;
    CollisionWorld& operator=(CollisionWorld&&) = delete;

    // --- Body management ---

    /**
     * Add a body to the collision world.
     * @return A handle for later removal/update, or kInvalidBodyHandle on failure.
     */
    [[nodiscard]] BodyHandle add_body(const BodyDef& def);

    /** Remove and destroy a body. */
    void remove_body(BodyHandle handle);

    /** Update the transform of a kinematic or dynamic body. */
    void set_body_transform(BodyHandle handle, const Vec3& position);

    /** Update transform with rotation (yaw in degrees around Y axis). */
    void set_body_transform(BodyHandle handle, const Vec3& position, float yaw_degrees);

    /** Get the current world-space position of a body. */
    [[nodiscard]] Vec3 get_body_position(BodyHandle handle) const;

    /** Get the user data from a body. */
    [[nodiscard]] u64 get_body_user_data(BodyHandle handle) const;

    /** Get the AABB that encloses a body. */
    [[nodiscard]] AABB get_body_aabb(BodyHandle handle) const;

    /** Enable or disable a body (disabled bodies are ignored by all queries). */
    void set_body_active(BodyHandle handle, bool active);

    // --- Simulation ---

    /**
     * Step the physics simulation forward.
     *
     * For server-authoritative mode: this should be called at a fixed
     * timestep (e.g. 60 Hz) to ensure deterministic results.
     *
     * @param delta_seconds  Fixed timestep duration.
     */
    void step(float delta_seconds);

    // --- Queries ---

    /**
     * Get all bodies whose AABB overlaps the given box.
     *
     * @param box      The query AABB.
     * @param mask     Only return bodies whose layer is in this mask.
     * @param out_handles  Output array for body handles.
     * @param max_handles  Capacity of out_handles.
     * @return         Number of handles written to out_handles.
     */
    [[nodiscard]] int query_aabb(
        const AABB& box,
        const CollisionMask& mask,
        BodyHandle* out_handles,
        int max_handles) const;

    // --- Stats ---

    [[nodiscard]] CollisionStats get_stats() const;

    // --- Character controller ---

    /**
     * Create a kinematic character controller that lives in this world.
     * The returned CharacterController shares this world's Jolt physics
     * system and responds to the same collision filters.
     */
    [[nodiscard]] std::unique_ptr<CharacterController> create_character(const CharacterDef& def);

    // --- Internal access (for trace backend implementation only) ---
    class Impl;
    [[nodiscard]] Impl* impl() { return impl_.get(); }
    [[nodiscard]] const Impl* impl() const { return impl_.get(); }

private:
    std::unique_ptr<Impl> impl_;
};

}  // namespace ae::collision
