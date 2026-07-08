#include "jolt_backend.h"  // Internal: defines CollisionWorld::Impl
#include "ae/core/log.h"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseQuery.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>

#define AE_LOG_CATEGORY "Collision"

namespace ae::collision {

// ============================================================
// CollisionWorld public API
// ============================================================

CollisionWorld::CollisionWorld()
    : impl_(std::make_unique<Impl>()) {
    log_info_cat(AE_LOG_CATEGORY, "CollisionWorld initialized");
}

CollisionWorld::~CollisionWorld() {
    if (impl_) {
        auto stats = get_stats();
        log_info_cat(AE_LOG_CATEGORY, "CollisionWorld destroyed (" + std::to_string(stats.body_count) +
                      " bodies, " + std::to_string(stats.active_dynamic_bodies) + " active dynamic)");
    }
}

BodyHandle CollisionWorld::add_body(const BodyDef& def) {
    JPH::ShapeRefC shape = impl_->create_shape(def.collider);
    if (!shape) {
        log_warning_cat(AE_LOG_CATEGORY, "add_body: shape creation failed (type=" +
                        std::to_string(static_cast<int>(def.collider.shape)) + ")");
        return kInvalidBodyHandle;
    }

    JPH::EMotionType motion_type = JPH::EMotionType::Static;
    JPH::ObjectLayer jolt_layer = jolt_helpers::to_jolt_layer(def.layer);

    switch (def.type) {
        case BodyType::Static:    motion_type = JPH::EMotionType::Static;    break;
        case BodyType::Kinematic: motion_type = JPH::EMotionType::Kinematic; break;
        case BodyType::Dynamic:   motion_type = JPH::EMotionType::Dynamic;   break;
    }

    JPH::BodyCreationSettings body_settings(
        shape,
        to_jolt_rvec3(def.position),
        JPH::Quat::sIdentity(),
        motion_type,
        jolt_layer
    );
    body_settings.mIsSensor = def.is_sensor;
    body_settings.mUserData = def.user_data;

    JPH::Body* body = impl_->physics_system.GetBodyInterface().CreateBody(body_settings);
    if (!body) {
        log_warning_cat(AE_LOG_CATEGORY, "add_body: Jolt body creation failed (layer=" +
                        std::to_string(static_cast<int>(def.layer)) + ")");
        return kInvalidBodyHandle;
    }

    impl_->physics_system.GetBodyInterface().AddBody(body->GetID(), JPH::EActivation::DontActivate);

    BodyHandle handle = impl_->next_handle++;
    InternalBody ib;
    ib.jolt_id = body->GetID();
    ib.def = def;
    ib.active = true;
    impl_->bodies[handle] = ib;

    log_debug_cat(AE_LOG_CATEGORY, "add_body: handle=" + std::to_string(handle) +
                  " type=" + std::to_string(static_cast<int>(def.type)) +
                  " layer=" + std::to_string(static_cast<int>(def.layer)) +
                  " pos=(" + std::to_string(def.position.x) + "," +
                  std::to_string(def.position.y) + "," +
                  std::to_string(def.position.z) + ")");
    return handle;
}

void CollisionWorld::remove_body(BodyHandle handle) {
    auto it = impl_->bodies.find(handle);
    if (it == impl_->bodies.end()) {
        log_trace_cat(AE_LOG_CATEGORY, "remove_body: unknown handle " + std::to_string(handle));
        return;
    }

    log_debug_cat(AE_LOG_CATEGORY, "remove_body: handle=" + std::to_string(handle) +
                  " type=" + std::to_string(static_cast<int>(it->second.def.type)));
    auto& bi = impl_->physics_system.GetBodyInterface();
    bi.RemoveBody(it->second.jolt_id);
    bi.DestroyBody(it->second.jolt_id);
    impl_->bodies.erase(it);
}

void CollisionWorld::set_body_transform(BodyHandle handle, const Vec3& position) {
    auto it = impl_->bodies.find(handle);
    if (it == impl_->bodies.end()) return;

    auto& bi = impl_->physics_system.GetBodyInterface();
    JPH::Quat rot = bi.GetRotation(it->second.jolt_id);
    bi.SetPositionAndRotation(it->second.jolt_id, to_jolt_rvec3(position), rot, JPH::EActivation::DontActivate);
}

void CollisionWorld::set_body_transform(BodyHandle handle, const Vec3& position, float yaw_degrees) {
    auto it = impl_->bodies.find(handle);
    if (it == impl_->bodies.end()) return;

    float yaw_rad = yaw_degrees * (pi / 180.0F);
    auto& bi = impl_->physics_system.GetBodyInterface();
    JPH::Quat rot = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), yaw_rad);
    bi.SetPositionAndRotation(it->second.jolt_id, to_jolt_rvec3(position), rot, JPH::EActivation::DontActivate);
}

Vec3 CollisionWorld::get_body_position(BodyHandle handle) const {
    auto it = impl_->bodies.find(handle);
    if (it == impl_->bodies.end()) return {};

    auto& bi = impl_->physics_system.GetBodyInterface();
    JPH::RVec3 pos = bi.GetPosition(it->second.jolt_id);
    return from_jolt_rvec3(pos);
}

u64 CollisionWorld::get_body_user_data(BodyHandle handle) const {
    auto it = impl_->bodies.find(handle);
    if (it == impl_->bodies.end()) return 0;

    auto& bi = impl_->physics_system.GetBodyInterface();
    return bi.GetUserData(it->second.jolt_id);
}

AABB CollisionWorld::get_body_aabb(BodyHandle handle) const {
    auto it = impl_->bodies.find(handle);
    if (it == impl_->bodies.end()) return {};

    auto& bi = impl_->physics_system.GetBodyInterface();
    JPH::TransformedShape ts = bi.GetTransformedShape(it->second.jolt_id);
    JPH::AABox jolt_box = ts.GetWorldSpaceBounds();

    return {
        {jolt_box.mMin.GetX(), jolt_box.mMin.GetY(), jolt_box.mMin.GetZ()},
        {jolt_box.mMax.GetX(), jolt_box.mMax.GetY(), jolt_box.mMax.GetZ()}
    };
}

void CollisionWorld::set_body_active(BodyHandle handle, bool active) {
    auto it = impl_->bodies.find(handle);
    if (it == impl_->bodies.end()) {
        log_trace_cat(AE_LOG_CATEGORY, "set_body_active: unknown handle " + std::to_string(handle));
        return;
    }

    if (it->second.active != active) {
        log_debug_cat(AE_LOG_CATEGORY, "set_body_active: handle=" + std::to_string(handle) +
                      " active=" + std::to_string(active));
    }
    it->second.active = active;
    if (!active) {
        impl_->physics_system.GetBodyInterface().DeactivateBody(it->second.jolt_id);
    }
}

void CollisionWorld::step(float delta_seconds) {
    log_trace_cat(AE_LOG_CATEGORY, "step: dt=" + std::to_string(delta_seconds));
    impl_->physics_system.Update(
        delta_seconds,
        1,
        &impl_->temp_allocator,
        &impl_->job_system
    );
}

int CollisionWorld::query_aabb(
    const AABB& box,
    const CollisionMask& mask,
    BodyHandle* out_handles,
    int max_handles) const
{
    JPH::AABox jolt_box(
        JPH::Vec3(box.min.x, box.min.y, box.min.z),
        JPH::Vec3(box.max.x, box.max.y, box.max.z)
    );

    const JPH::BroadPhaseQuery& bp_query = impl_->physics_system.GetBroadPhaseQuery();

    AllPassBroadPhaseLayerFilter bp_filter;
    AllPassObjectLayerFilter obj_filter;

    JPH::AllHitCollisionCollector<JPH::CollideShapeBodyCollector> collector;
    bp_query.CollideAABox(jolt_box, collector, bp_filter, obj_filter);

    int count = 0;
    for (const auto& jolt_id : collector.mHits) {
        if (count >= max_handles) break;

        for (const auto& [handle, body] : impl_->bodies) {
            if (body.jolt_id == jolt_id && body.active) {
                if (mask.test(body.def.layer)) {
                    out_handles[count++] = handle;
                }
                break;
            }
        }
    }
    return count;
}

CollisionStats CollisionWorld::get_stats() const {
    CollisionStats stats;
    stats.body_count = static_cast<int>(impl_->bodies.size());

    auto& bi = impl_->physics_system.GetBodyInterface();
    for (const auto& [handle, body] : impl_->bodies) {
        if (body.def.type == BodyType::Dynamic && body.active) {
            if (bi.IsActive(body.jolt_id)) {
                ++stats.active_dynamic_bodies;
            }
        }
    }

    stats.broadphase_pairs = stats.body_count * 2;
    stats.narrowphase_contacts = 0;

    return stats;
}

}  // namespace ae::collision
