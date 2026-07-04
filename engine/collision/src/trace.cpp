#include "ae/collision/trace.h"
#include "jolt_backend.h"  // Internal: gives us CollisionWorld::Impl, helpers, and filters
#include "ae/core/log.h"

#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/CollideShape.h>

#include <cmath>

#define AE_LOG_CATEGORY "Collision"

namespace ae::collision {

namespace {

// ============================================================
// Body filter that respects our CollisionMask
// ============================================================
class MaskBodyFilter : public JPH::BodyFilter {
public:
    MaskBodyFilter(const CollisionWorld::Impl* impl,
                   const CollisionMask& mask,
                   u64 ignore_user_data)
        : impl_(impl), mask_(mask), ignore_user_data_(ignore_user_data) {}

    bool ShouldCollide(const JPH::BodyID& inBodyID) const override {
        if (ignore_user_data_ != 0) {
            auto& bl = impl_->physics_system.GetBodyLockInterface();
            JPH::BodyLockRead lock(bl, inBodyID);
            if (lock.Succeeded()) {
                if (lock.GetBody().GetUserData() == ignore_user_data_) return false;
            }
        }

        for (const auto& [handle, body] : impl_->bodies) {
            if (body.jolt_id == inBodyID && body.active) {
                return mask_.test(body.def.layer);
            }
        }
        return false;
    }

    bool ShouldCollideLocked(const JPH::Body& inBody) const override {
        if (ignore_user_data_ != 0) {
            if (inBody.GetUserData() == ignore_user_data_) return false;
        }
        JPH::BodyID id = inBody.GetID();
        for (const auto& [handle, body] : impl_->bodies) {
            if (body.jolt_id == id && body.active) {
                return mask_.test(body.def.layer);
            }
        }
        return false;
    }

private:
    const CollisionWorld::Impl* impl_;
    const CollisionMask& mask_;
    u64 ignore_user_data_;
};

}  // namespace

// ============================================================
// ray_trace
// ============================================================
TraceResult ray_trace(
    const CollisionWorld& world,
    const Ray& ray,
    const TraceParams& params)
{
    TraceResult result {};

    const auto* impl = world.impl();
    if (!impl) {
        log_warning_cat(AE_LOG_CATEGORY, "ray_trace: world impl is null");
        return result;
    }

    JPH::RRayCast jolt_ray {
        to_jolt_rvec3(ray.origin),
        to_jolt_vec3(ray.direction * params.max_distance)
    };

    AllPassBroadPhaseLayerFilter bp_filter;
    AllPassObjectLayerFilter obj_filter;
    MaskBodyFilter body_filter(impl, params.layer_mask, params.ignore_user_data);

    JPH::RayCastResult hit;
    bool has_hit = impl->physics_system.GetNarrowPhaseQuery().CastRay(
        jolt_ray,
        hit,
        bp_filter,
        obj_filter,
        body_filter
    );

    if (has_hit) {
        result.hit = true;
        result.distance = hit.mFraction * params.max_distance;

        JPH::RVec3 hit_point = jolt_ray.GetPointOnRay(hit.mFraction);
        result.position = from_jolt_rvec3(hit_point);

        auto& bl = impl->physics_system.GetBodyLockInterface();
        JPH::BodyLockRead lock(bl, hit.mBodyID);
        if (lock.Succeeded()) {
            JPH::Vec3 normal = lock.GetBody().GetWorldSpaceSurfaceNormal(
                hit.mSubShapeID2,
                hit_point
            );
            result.normal = from_jolt_vec3(normal);
        }

        result.body_index = hit.mBodyID.GetIndexAndSequenceNumber();
        log_trace_cat(AE_LOG_CATEGORY, "ray_trace: hit dist=" + std::to_string(result.distance) +
                      " body=" + std::to_string(result.body_index));
    } else {
        log_trace_cat(AE_LOG_CATEGORY, "ray_trace: miss");
    }

    return result;
}

// ============================================================
// sphere_trace
// ============================================================
TraceResult sphere_trace(
    const CollisionWorld& world,
    const Vec3& start,
    const Vec3& direction,
    float distance,
    float radius,
    const TraceParams& params)
{
    TraceResult result {};

    const auto* impl = world.impl();
    if (!impl) {
        log_warning_cat(AE_LOG_CATEGORY, "sphere_trace: world impl is null");
        return result;
    }

    JPH::SphereShape sphere_shape(radius);

    JPH::RShapeCast shape_cast(
        &sphere_shape,
        JPH::Vec3::sReplicate(1.0F),
        JPH::RMat44::sTranslation(to_jolt_rvec3(start)),
        to_jolt_vec3(direction * distance)
    );

    JPH::ShapeCastSettings settings;
    settings.mBackFaceModeTriangles = params.ignore_backfaces
        ? JPH::EBackFaceMode::IgnoreBackFaces
        : JPH::EBackFaceMode::CollideWithBackFaces;

    AllPassBroadPhaseLayerFilter bp_filter;
    AllPassObjectLayerFilter obj_filter;
    MaskBodyFilter body_filter(impl, params.layer_mask, params.ignore_user_data);

    JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
    impl->physics_system.GetNarrowPhaseQuery().CastShape(
        shape_cast,
        settings,
        JPH::RVec3::sZero(),
        collector,
        bp_filter,
        obj_filter,
        body_filter
    );

    if (collector.HadHit()) {
        const auto& hit = collector.mHit;
        result.hit = true;
        result.distance = hit.mFraction * distance;

        JPH::RVec3 hit_point = shape_cast.GetPointOnRay(hit.mFraction);
        result.position = from_jolt_rvec3(hit_point);

        // PenetrationAxis points shape1→shape2 (into hit body).
        // Normal pointing out of hit body is -axis.
        result.normal = from_jolt_vec3(-hit.mPenetrationAxis.Normalized());
        result.body_index = hit.mBodyID2.GetIndexAndSequenceNumber();
        log_trace_cat(AE_LOG_CATEGORY, "sphere_trace: hit dist=" + std::to_string(result.distance) +
                      " body=" + std::to_string(result.body_index));
    } else {
        log_trace_cat(AE_LOG_CATEGORY, "sphere_trace: miss");
    }

    return result;
}

// ============================================================
// capsule_trace
// ============================================================
TraceResult capsule_trace(
    const CollisionWorld& world,
    const Vec3& start,
    const Vec3& direction,
    float distance,
    float half_height,
    float radius,
    const TraceParams& params)
{
    TraceResult result {};

    const auto* impl = world.impl();
    if (!impl) {
        log_warning_cat(AE_LOG_CATEGORY, "capsule_trace: world impl is null");
        return result;
    }

    JPH::CapsuleShape capsule_shape(half_height, radius);

    JPH::RShapeCast shape_cast(
        &capsule_shape,
        JPH::Vec3::sReplicate(1.0F),
        JPH::RMat44::sTranslation(to_jolt_rvec3(start)),
        to_jolt_vec3(direction * distance)
    );

    JPH::ShapeCastSettings settings;
    settings.mBackFaceModeTriangles = params.ignore_backfaces
        ? JPH::EBackFaceMode::IgnoreBackFaces
        : JPH::EBackFaceMode::CollideWithBackFaces;

    AllPassBroadPhaseLayerFilter bp_filter;
    AllPassObjectLayerFilter obj_filter;
    MaskBodyFilter body_filter(impl, params.layer_mask, params.ignore_user_data);

    JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
    impl->physics_system.GetNarrowPhaseQuery().CastShape(
        shape_cast,
        settings,
        JPH::RVec3::sZero(),
        collector,
        bp_filter,
        obj_filter,
        body_filter
    );

    if (collector.HadHit()) {
        const auto& hit = collector.mHit;
        result.hit = true;
        result.distance = hit.mFraction * distance;

        JPH::RVec3 hit_point = shape_cast.GetPointOnRay(hit.mFraction);
        result.position = from_jolt_rvec3(hit_point);
        result.normal = from_jolt_vec3(-hit.mPenetrationAxis.Normalized());
        result.body_index = hit.mBodyID2.GetIndexAndSequenceNumber();
        log_trace_cat(AE_LOG_CATEGORY, "capsule_trace: hit dist=" + std::to_string(result.distance) +
                      " body=" + std::to_string(result.body_index));
    } else {
        log_trace_cat(AE_LOG_CATEGORY, "capsule_trace: miss");
    }

    return result;
}

// ============================================================
// sphere_overlap
// ============================================================
bool sphere_overlap(
    const CollisionWorld& world,
    const Vec3& center,
    float radius,
    const TraceParams& params)
{
    const auto* impl = world.impl();
    if (!impl) {
        log_warning_cat(AE_LOG_CATEGORY, "sphere_overlap: world impl is null");
        return false;
    }

    JPH::SphereShape sphere_shape(radius);

    AllPassBroadPhaseLayerFilter bp_filter;
    AllPassObjectLayerFilter obj_filter;
    MaskBodyFilter body_filter(impl, params.layer_mask, params.ignore_user_data);

    JPH::AnyHitCollisionCollector<JPH::CollideShapeCollector> collector;
    impl->physics_system.GetNarrowPhaseQuery().CollideShape(
        &sphere_shape,
        JPH::Vec3::sReplicate(1.0F),
        JPH::RMat44::sTranslation(to_jolt_rvec3(center)),
        JPH::CollideShapeSettings(),
        JPH::RVec3::sZero(),
        collector,
        bp_filter,
        obj_filter,
        body_filter
    );

    bool had_hit = collector.HadHit();
    log_trace_cat(AE_LOG_CATEGORY, "sphere_overlap: " + std::string(had_hit ? "hit" : "miss"));
    return had_hit;
}

// ============================================================
// aabb_overlap
// ============================================================
bool aabb_overlap(
    const CollisionWorld& world,
    const AABB& box,
    const TraceParams& params)
{
    BodyHandle handles[64];
    int count = world.query_aabb(box, params.layer_mask, handles, 64);
    log_trace_cat(AE_LOG_CATEGORY, "aabb_overlap: " + std::to_string(count) + " bodies in region");
    return count > 0;
}

// ============================================================
// cone_trace_closest
// ============================================================
TraceResult cone_trace_closest(
    const CollisionWorld& world,
    const Vec3& origin,
    const Vec3& forward,
    float cone_half_angle_deg,
    int num_traces,
    float max_distance,
    const TraceParams& params,
    u64& rng_state)
{
    TraceResult closest {};
    closest.distance = max_distance;

    const float half_angle_rad = cone_half_angle_deg * (pi / 180.0F);

    auto next_rand = [&rng_state]() -> float {
        rng_state ^= rng_state << 13;
        rng_state ^= rng_state >> 7;
        rng_state ^= rng_state << 17;
        return static_cast<float>(rng_state & 0x007FFFFF) / 8388608.0F;
    };

    Vec3 up {0.0F, 1.0F, 0.0F};
    Vec3 right = cross(up, forward).normalized();
    if (right.length_squared() < 0.001F) {
        right = cross(Vec3{1.0F, 0.0F, 0.0F}, forward).normalized();
    }
    Vec3 local_up = cross(forward, right).normalized();

    for (int i = 0; i < num_traces; ++i) {
        float r = next_rand();
        float angle = half_angle_rad * std::sqrt(r);
        float phi = next_rand() * 2.0F * pi;

        float cone_x = std::sin(angle) * std::cos(phi);
        float cone_y = std::sin(angle) * std::sin(phi);
        float cone_z = std::cos(angle);

        Vec3 dir = right * cone_x + local_up * cone_y + forward * cone_z;
        dir = dir.normalized();

        Ray ray {origin, dir};
        TraceParams tp = params;
        tp.max_distance = max_distance;

        TraceResult hit = ray_trace(world, ray, tp);
        if (hit.hit && hit.distance < closest.distance) {
            closest = hit;
        }
    }

    log_trace_cat(AE_LOG_CATEGORY, "cone_trace_closest: " + std::to_string(num_traces) + " traces, " +
                  (closest.hit ? "hit dist=" + std::to_string(closest.distance) : "miss"));
    return closest;
}

// ============================================================
// resolve_hitbox_hurtbox_overlaps
// ============================================================
int resolve_hitbox_hurtbox_overlaps(
    const HitboxInstance* hitboxes, int hitbox_count,
    const HitboxInstance* hurtboxes, int hurtbox_count,
    int* out_hit_indices, int* out_hurt_indices,
    int max_hits)
{
    int count = 0;

    for (int hb = 0; hb < hitbox_count && count < max_hits; ++hb) {
        const auto& hitbox = hitboxes[hb];
        if (!hitbox.active || hitbox.type != HitboxType::Hitbox) continue;

        for (int hu = 0; hu < hurtbox_count && count < max_hits; ++hu) {
            const auto& hurtbox = hurtboxes[hu];
            if (!hurtbox.active || hurtbox.type != HitboxType::Hurtbox) continue;

            if (hitbox.owner_entity == hurtbox.owner_entity) continue;

            if (hitbox.box.overlaps(hurtbox.box)) {
                out_hit_indices[count] = hb;
                out_hurt_indices[count] = hu;
                ++count;
                break;
            }
        }
    }

    log_trace_cat(AE_LOG_CATEGORY, "resolve_hitbox_hurtbox: " + std::to_string(hitbox_count) + " hitboxes, " +
                  std::to_string(hurtbox_count) + " hurtboxes -> " + std::to_string(count) + " overlaps");
    return count;
}

}  // namespace ae::collision
