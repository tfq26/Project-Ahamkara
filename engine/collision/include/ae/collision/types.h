#pragma once

#include "ae/core/math.h"
#include "ae/core/types.h"

#include <cstdint>
#include <type_traits>

namespace ae::collision {

// ============================================================
// Primitive collision shapes (data-only, no physics backend)
// ============================================================

/** A ray with origin and direction. Direction is expected to be normalized. */
struct Ray {
    Vec3 origin {};
    Vec3 direction {0.0F, 0.0F, 1.0F};
};

/** A sphere defined by center and radius. */
struct Sphere {
    Vec3 center {};
    float radius {0.5F};
};

/** A capsule defined by its two end caps and radius. */
struct Capsule {
    Vec3 top {};     // high point
    Vec3 bottom {};  // low point  
    float radius {0.5F};
};

/** An axis-aligned bounding box. */
struct AABB {
    Vec3 min {};
    Vec3 max {};

    [[nodiscard]] Vec3 center() const {
        return {
            (min.x + max.x) * 0.5F,
            (min.y + max.y) * 0.5F,
            (min.z + max.z) * 0.5F
        };
    }

    [[nodiscard]] Vec3 extents() const {
        return {
            (max.x - min.x) * 0.5F,
            (max.y - min.y) * 0.5F,
            (max.z - min.z) * 0.5F
        };
    }

    [[nodiscard]] bool contains(const Vec3& point) const {
        return point.x >= min.x && point.x <= max.x
            && point.y >= min.y && point.y <= max.y
            && point.z >= min.z && point.z <= max.z;
    }

    [[nodiscard]] bool overlaps(const AABB& other) const {
        return min.x <= other.max.x && max.x >= other.min.x
            && min.y <= other.max.y && max.y >= other.min.y
            && min.z <= other.max.z && max.z >= other.min.z;
    }

    /** Expand this AABB to include the given point. */
    void expand(const Vec3& point) {
        if (point.x < min.x) min.x = point.x;
        if (point.y < min.y) min.y = point.y;
        if (point.z < min.z) min.z = point.z;
        if (point.x > max.x) max.x = point.x;
        if (point.y > max.y) max.y = point.y;
        if (point.z > max.z) max.z = point.z;
    }

    /** Create an empty (inverted) AABB ready for expansion. */
    [[nodiscard]] static AABB make_empty() {
        constexpr float inf = 1.0e30F;
        return {{inf, inf, inf}, {-inf, -inf, -inf}};
    }
};

/** Three vertices defining a triangle for static mesh collision. */
struct Triangle {
    Vec3 v0 {};
    Vec3 v1 {};
    Vec3 v2 {};

    [[nodiscard]] Vec3 normal() const {
        return cross(v1 - v0, v2 - v0).normalized();
    }
};

// ============================================================
// Trace / query results
// ============================================================

/** Result of any shape trace (ray, sphere, capsule sweep). */
struct TraceResult {
    bool hit {false};          ///< Whether anything was hit
    float distance {0.0F};     ///< Distance along the trace direction to hit point
    Vec3 position {};          ///< World-space hit position
    Vec3 normal {};            ///< Surface normal at hit point
    u64 user_data {0};         ///< Application-defined data on the hit object
    u32 body_index {0};        ///< Index of the body that was hit
};

// ============================================================
// Trigger volumes
// ============================================================

/** A volume that fires callbacks when objects enter/leave. */
enum class TriggerShape : u8 {
    Box,
    Sphere,
    Capsule,
};

struct TriggerVolumeDef {
    TriggerShape shape {TriggerShape::Box};
    Vec3 half_extents {0.5F, 0.5F, 0.5F};  // for Box
    float radius {1.0F};                     // for Sphere/Capsule
    float capsule_half_height {1.0F};        // for Capsule
    Vec3 position {};
    u32 trigger_id {0};
    bool one_shot {false};                   ///< Fire only once then deactivate
};

struct TriggerEvent {
    u32 trigger_id {0};
    u32 body_index {0};
    u64 body_user_data {0};
    bool entered {true};  ///< true = entered volume, false = left
};

// ============================================================
// Hitboxes and hurtboxes
// ============================================================

/**
 * A hitbox is attached to a character/entity for damage *dealing* (weapon).
 * A hurtbox is attached to a character/entity for damage *receiving*.
 *
 * Separation allows different shapes for attacking vs being hit.
 * For example, a melee weapon may have a large capsule hitbox that sweeps
 * in an arc, while the character hurtbox is a tighter capsule around the torso.
 */
enum class HitboxType : u8 {
    Hitbox,   ///< Deals damage (weapon swings, projectiles)
    Hurtbox,  ///< Receives damage (character body, weak points)
};

/** World-space box used for both hitboxes and hurtboxes. */
struct HitboxInstance {
    HitboxType type {HitboxType::Hurtbox};
    AABB box {};                  ///< World-space bounds for this tick
    u32 owner_entity {0};         ///< Entity that owns this box
    u32 box_index {0};            ///< Which box on the entity (0=body, 1=head, etc.)
    float damage_multiplier {1.0F}; ///< 2.0 = headshot, 1.0 = body, etc.
    bool active {true};
};

}  // namespace ae::collision

static_assert(std::is_trivially_copyable_v<ae::collision::Ray>);
static_assert(std::is_trivially_copyable_v<ae::collision::Sphere>);
static_assert(std::is_trivially_copyable_v<ae::collision::AABB>);
static_assert(std::is_trivially_copyable_v<ae::collision::TraceResult>);
