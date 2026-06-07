#pragma once

#include "ae/collision/types.h"
#include "ae/collision/layers.h"

namespace ae::collision {

// ============================================================
// Tracing / collision query API
//
// This is a thin, backend-agnostic interface for performing
// spatial queries against the collision world.
//
// The collision world implementation (Jolt-backed) lives in
// engine/collision/src/ and is not visible to game code that
// includes this header.
// ============================================================

// Forward declaration of the opaque backend
class CollisionWorld;

/** Parameters controlling a trace operation. */
struct TraceParams {
    CollisionMask layer_mask {};      ///< Only hit bodies whose layer is in this mask
    bool ignore_backfaces {true};     ///< Ignore back-facing triangles
    u64 ignore_user_data {0};         ///< If non-zero, skip bodies with this user data
    float max_distance {1000.0F};     ///< Maximum trace distance
};

// --- Trace functions ---

/**
 * Cast a ray through the collision world and return the closest hit.
 *
 * @param world  The collision world to query.
 * @param ray    Origin and (normalized) direction of the ray.
 * @param params Filter/limit parameters for the trace.
 * @return       Details of the closest hit, or hit=false if nothing was struck.
 */
[[nodiscard]] TraceResult ray_trace(
    const CollisionWorld& world,
    const Ray& ray,
    const TraceParams& params = {});

/**
 * Sweep a sphere through the collision world and return the closest hit.
 *
 * The sphere starts at `start` and sweeps along `direction` by `distance`.
 * The trace is a linear sweep (no rotational velocity).
 *
 * @param world     The collision world to query.
 * @param start     Center of the sphere at the start of the sweep.
 * @param direction Normalized direction of the sweep.
 * @param distance  How far to sweep (in world units).
 * @param radius    Radius of the sphere.
 * @param params    Filter/limit parameters for the trace.
 * @return          Details of the closest hit, or hit=false if nothing was struck.
 */
[[nodiscard]] TraceResult sphere_trace(
    const CollisionWorld& world,
    const Vec3& start,
    const Vec3& direction,
    float distance,
    float radius,
    const TraceParams& params = {});

/**
 * Sweep a capsule through the collision world and return the closest hit.
 *
 * The capsule is defined by its center at `start`, a half-height,
 * and a radius. It sweeps along `direction` by `distance`.
 *
 * @param world       The collision world to query.
 * @param start       Center of the capsule at the start of the sweep.
 * @param direction   Normalized direction of the sweep.
 * @param distance    How far to sweep.
 * @param half_height Half the height of the capsule (end-to-end / 2).
 * @param radius      Radius of the capsule.
 * @param params      Filter/limit parameters.
 * @return            Details of the closest hit.
 */
[[nodiscard]] TraceResult capsule_trace(
    const CollisionWorld& world,
    const Vec3& start,
    const Vec3& direction,
    float distance,
    float half_height,
    float radius,
    const TraceParams& params = {});

// --- Overlap queries ---

/**
 * Test if a sphere overlaps any collision bodies.
 *
 * @param world   The collision world to query.
 * @param center  Center of the sphere.
 * @param radius  Radius of the sphere.
 * @param params  Filter/limit parameters.
 * @return        true if any eligible body overlaps the sphere.
 */
[[nodiscard]] bool sphere_overlap(
    const CollisionWorld& world,
    const Vec3& center,
    float radius,
    const TraceParams& params = {});

/**
 * Test if an AABB overlaps any collision bodies.
 *
 * @param world  The collision world to query.
 * @param box    The axis-aligned box to test.
 * @param params Filter/limit parameters.
 * @return       true if any eligible body overlaps the box.
 */
[[nodiscard]] bool aabb_overlap(
    const CollisionWorld& world,
    const AABB& box,
    const TraceParams& params = {});

// --- Helper: trace that stops early (for melee / cone attacks) ---

/**
 * Perform multiple traces from a single origin in a cone and return
 * the first result that hits within the max distance.
 *
 * Useful for melee attacks, shotgun spread, etc.
 *
 * @param world         The collision world.
 * @param origin        Starting point of all traces.
 * @param forward       Center direction of the cone.
 * @param cone_half_angle_deg  Half-angle of the cone in degrees.
 * @param num_traces    How many rays to fire within the cone.
 * @param max_distance  Max trace distance.
 * @param params        Filter/limit parameters.
 * @param rng_state     In/out random state for distributing rays within cone.
 * @return              The closest hit result across all traces.
 */
[[nodiscard]] TraceResult cone_trace_closest(
    const CollisionWorld& world,
    const Vec3& origin,
    const Vec3& forward,
    float cone_half_angle_deg,
    int num_traces,
    float max_distance,
    const TraceParams& params,
    u64& rng_state);

// --- Utility: sweep a hitbox/hurtbox ---

/**
 * Compute all hurtboxes that overlap with a given hitbox.
 *
 * This is the core loop for resolving melee and projectile damage.
 *
 * @param hitboxes     List of all active hitboxes this tick (weapon swings).
 * @param hitbox_count Number of hitboxes.
 * @param hurtboxes    List of all active hurtboxes this tick (character bodies).
 * @param hurtbox_count Number of hurtboxes.
 * @param out_hits     Output: up to max_hits pairs of (hitbox_index, hurtbox_index).
 * @param max_hits     Capacity of out_hits.
 * @return             Number of overlaps written to out_hits.
 */
[[nodiscard]] int resolve_hitbox_hurtbox_overlaps(
    const HitboxInstance* hitboxes, int hitbox_count,
    const HitboxInstance* hurtboxes, int hurtbox_count,
    int* out_hit_indices, int* out_hurt_indices,
    int max_hits);

}  // namespace ae::collision
