#pragma once

#include "ae/core/types.h"

namespace ae::collision {

// ============================================================
// Physics layers and collision masks
//
// Layers partition the world into independent collision groups.
// Masks select which layers a query or body can interact with.
//
// Architecture inspired by Source / Overwatch layer design:
//   - 64-bit mask allows up to 64 independent layers
//   - Each body belongs to exactly one layer
//   - Each body has a mask of layers it collides with
//   - Queries also have a mask to filter what they hit
// ============================================================

/** A single collision layer index (0..63). */
using CollisionLayer = u8;

/** A 64-bit bitmask of collision layers. */
struct CollisionMask {
    u64 bits {0xFFFFFFFFFFFFFFFFULL};  ///< Default: collide with everything

    /** Set a layer bit in the mask. */
    void set(CollisionLayer layer) {
        if (layer < 64) bits |= (1ULL << layer);
    }

    /** Clear a layer bit in the mask. */
    void clear(CollisionLayer layer) {
        if (layer < 64) bits &= ~(1ULL << layer);
    }

    /** Test if a layer bit is set. */
    [[nodiscard]] bool test(CollisionLayer layer) const {
        if (layer >= 64) return false;
        return (bits & (1ULL << layer)) != 0;
    }

    /** Test if this mask collides with another (any overlapping bits). */
    [[nodiscard]] bool overlaps(const CollisionMask& other) const {
        return (bits & other.bits) != 0;
    }

    /** Combine masks (union). */
    CollisionMask& operator|=(const CollisionMask& other) {
        bits |= other.bits;
        return *this;
    }

    [[nodiscard]] CollisionMask operator|(CollisionMask other) const {
        other.bits |= bits;
        return other;
    }

    [[nodiscard]] bool operator==(const CollisionMask& other) const = default;
};

// --- Standard game layers (0..15 reserved for engine, 16..63 for game) ---

/** Pre-defined collision layers that every Ahamkara entity can use. */
struct GameLayers {
    // Engine-reserved layers
    static constexpr CollisionLayer WORLD_STATIC  = 0;   ///< Static world geometry
    static constexpr CollisionLayer WORLD_DYNAMIC = 1;   ///< Movable world objects (doors, platforms)
    static constexpr CollisionLayer PLAYER        = 2;   ///< Player characters
    static constexpr CollisionLayer NPC           = 3;   ///< AI enemies
    static constexpr CollisionLayer PROJECTILE    = 4;   ///< Bullets, rockets
    static constexpr CollisionLayer TRIGGER       = 5;   ///< Trigger volumes
    static constexpr CollisionLayer PICKUP        = 6;   ///< Weapons, health packs
    static constexpr CollisionLayer VEHICLE       = 7;   ///< Vehicles
    static constexpr CollisionLayer DEBRIS        = 8;   ///< Physics debris (no gameplay collision)
    static constexpr CollisionLayer CAMERA        = 9;   ///< Camera raycasts only

    // Game-specific layers start at 16
    static constexpr CollisionLayer GAME_LAYER_FIRST = 16;

    // Common pre-built masks
    [[nodiscard]] static CollisionMask player_mask() {
        CollisionMask m{}; m.bits = 0;
        m.set(WORLD_STATIC);
        m.set(WORLD_DYNAMIC);
        m.set(PLAYER);
        m.set(NPC);
        m.set(TRIGGER);
        m.set(PICKUP);
        m.set(VEHICLE);
        return m;
    }

    [[nodiscard]] static CollisionMask projectile_mask() {
        CollisionMask m{}; m.bits = 0;
        m.set(WORLD_STATIC);
        m.set(WORLD_DYNAMIC);
        m.set(PLAYER);
        m.set(NPC);
        m.set(VEHICLE);
        return m;
    }

    [[nodiscard]] static CollisionMask camera_mask() {
        CollisionMask m{}; m.bits = 0;
        m.set(WORLD_STATIC);
        m.set(WORLD_DYNAMIC);
        return m;
    }

    [[nodiscard]] static CollisionMask trigger_mask() {
        CollisionMask m{}; m.bits = 0;
        m.set(PLAYER);
        m.set(NPC);
        m.set(PROJECTILE);
        return m;
    }
};

}  // namespace ae::collision
