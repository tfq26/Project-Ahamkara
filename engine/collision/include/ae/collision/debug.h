#pragma once

#include "ae/collision/types.h"
#include "ae/collision/layers.h"

namespace ae::collision {

// ============================================================
// Debug collision overlay types
//
// These are pure-data structures that the collision system
// populates each tick. The renderer reads them to draw debug
// wireframe overlays. No render dependencies here.
// ============================================================

/** A single debug wireframe box for a collision body or hitbox. */
struct DebugBox {
    Vec3 min {};
    Vec3 max {};
    float red {0.0F};
    float green {1.0F};
    float blue {0.0F};
    float alpha {0.5F};
};

/** A debug line segment (e.g. ray trace, surface normal). */
struct DebugLine {
    Vec3 start {};
    Vec3 end {};
    float red {1.0F};
    float green {0.0F};
    float blue {0.0F};
};

/** A debug sphere (wireframe circle rendered at position). */
struct DebugSphere {
    Vec3 center {};
    float radius {0.5F};
    float red {0.0F};
    float green {0.0F};
    float blue {1.0F};
};

/** Color presets for different collision categories. */
namespace DebugColors {
    inline void static_world(float& r, float& g, float& b) { r = 0.25F; g = 0.30F; b = 0.36F; }
    inline void dynamic_world(float& r, float& g, float& b) { r = 0.50F; g = 0.55F; b = 0.60F; }
    inline void player(float& r, float& g, float& b)          { r = 0.00F; g = 0.80F; b = 0.00F; }
    inline void hitbox(float& r, float& g, float& b)          { r = 1.00F; g = 0.20F; b = 0.20F; }
    inline void hurtbox(float& r, float& g, float& b)         { r = 0.00F; g = 0.50F; b = 1.00F; }
    inline void trigger(float& r, float& g, float& b)         { r = 1.00F; g = 0.80F; b = 0.00F; }
    inline void projectile(float& r, float& g, float& b)      { r = 0.80F; g = 0.20F; b = 0.80F; }
    inline void trace_ray(float& r, float& g, float& b)       { r = 1.00F; g = 0.50F; b = 0.00F; }
    inline void trace_hit(float& r, float& g, float& b)       { r = 0.00F; g = 1.00F; b = 0.00F; }
    inline void sensor(float& r, float& g, float& b)          { r = 0.00F; g = 0.80F; b = 0.80F; }
    inline void debris(float& r, float& g, float& b)          { r = 0.50F; g = 0.40F; b = 0.30F; }
    inline void vehicle(float& r, float& g, float& b)         { r = 0.40F; g = 0.20F; b = 0.60F; }
    inline void npc(float& r, float& g, float& b)             { r = 0.80F; g = 0.20F; b = 0.20F; }
    inline void pickup(float& r, float& g, float& b)          { r = 0.00F; g = 1.00F; b = 0.60F; }
}

/**
 * Maximum counts for debug overlay arrays (per-tick budget).
 * Tune these based on expected game complexity.
 */
constexpr int kMaxDebugBoxes   = 256;
constexpr int kMaxDebugLines   = 256;
constexpr int kMaxDebugSpheres = 128;

/**
 * Collection of debug primitives for one frame.
 * Populated by the collision system, consumed by the renderer.
 */
struct DebugOverlay {
    int box_count {0};
    DebugBox boxes[kMaxDebugBoxes] {};

    int line_count {0};
    DebugLine lines[kMaxDebugLines] {};

    int sphere_count {0};
    DebugSphere spheres[kMaxDebugSpheres] {};

    // Collision stats as a human-readable string
    const char* stats_text {nullptr};

    /** Clear all geometry for the next frame. */
    void clear() {
        box_count = 0;
        line_count = 0;
        sphere_count = 0;
        stats_text = nullptr;
    }
};

// ============================================================
// Debug overlay population functions
// ============================================================

class CollisionWorld;
struct Ray;
struct TraceResult;
struct HitboxInstance;

/** Populate a debug overlay with all collision bodies in the world. */
void populate_debug_overlay(CollisionWorld& world, DebugOverlay& overlay);

/** Populate a debug overlay with hitbox/hurtbox wireframes. */
void populate_debug_hitboxes(const HitboxInstance* hitboxes, int hitbox_count,
                             DebugOverlay& overlay);

/** Populate a debug overlay with trace visualization (ray + hit point + normal). */
void populate_debug_trace(const Ray& ray, const TraceResult& result,
                          DebugOverlay& overlay);

}  // namespace ae::collision
