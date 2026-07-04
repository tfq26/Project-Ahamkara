#include "ae/physics/physics_world.h"
#include "ae/core/log.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

namespace ae::physics {
namespace {

// Jolt physics layers
namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
};

namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
};

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }
    JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        return mObjectToBroadPhase[inLayer];
    }
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer) const override { return "UNKNOWN"; }
private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        return true; // all layers collide
    }
};

class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override {
        return true;
    }
};

struct JoltBody {
    JPH::BodyID jolt_id;
    JPH::Body* jolt_body {nullptr};
    bool jump_through {false};
};

struct JoltCharacter {
    JPH::RefConst<JPH::Shape> standing_shape;
    JPH::RefConst<JPH::Shape> crouching_shape;
    JPH::CharacterVirtual* character {nullptr};
    float current_radius {0.22F};
    float current_height {1.70F};
};

// Contact listener for jump-through rejection
class CharacterContactListener : public JPH::CharacterContactListener {
public:
    std::vector<JoltBody>* bodies {nullptr};

    bool OnContactValidate(const JPH::CharacterVirtual* inCharacter,
                           const JPH::BodyID& inBodyID2,
                           const JPH::SubShapeID&) override {
        (void)inCharacter;
        if (!bodies) return true;

        JPH::uint32 idx = inBodyID2.GetIndex();
        if (idx < bodies->size()) {
            const auto& body = (*bodies)[idx];
            if (body.jump_through) {
                if (inCharacter->GetLinearVelocity().GetY() > 0.01F) {
                    return false; // reject — character moving up through jump-through
                }
            }
        }
        return true;
    }
};

}  // namespace

#define AE_LOG_CATEGORY "Physics"

struct PhysicsWorld::Impl {
    JPH::TempAllocatorImpl temp_allocator {10 * 1024 * 1024};
    JPH::JobSystemThreadPool job_system {JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, 1};
    BPLayerInterfaceImpl bp_layer_interface;
    ObjectVsBroadPhaseLayerFilterImpl ob_bp_filter;
    ObjectLayerPairFilterImpl ob_ob_filter;
    JPH::PhysicsSystem physics_system;
    CharacterContactListener contact_listener;
    std::vector<JoltBody> bodies;
    std::vector<JoltCharacter> characters;

    Impl() {
        physics_system.Init(
            1024, 0, 1024, 1024,
            bp_layer_interface,
            ob_bp_filter,
            ob_ob_filter
        );
        contact_listener.bodies = &bodies;
        log_info_cat(AE_LOG_CATEGORY, "PhysicsSystem initialized (max bodies=1024, caches=1024, temp=10MB, worker_threads=1)");
    }

    ~Impl() {
        std::size_t char_count = 0;
        for (auto& ch : characters) {
            delete ch.character;
            ++char_count;
        }
        characters.clear();

        std::size_t body_count = 0;
        auto& bi = physics_system.GetBodyInterface();
        for (auto& body : bodies) {
            if (!body.jolt_id.IsInvalid()) {
                bi.RemoveBody(body.jolt_id);
                bi.DestroyBody(body.jolt_id);
                ++body_count;
            }
        }
        bodies.clear();
        log_info_cat(AE_LOG_CATEGORY, "PhysicsSystem shutdown: removed " + std::to_string(body_count) +
                      " bodies, deleted " + std::to_string(char_count) + " characters");
    }
};

// --- One-time Jolt init -------------------------------------------------------
namespace {
    bool s_jolt_registered = false;
    void ensure_jolt() {
        if (!s_jolt_registered) {
            JPH::RegisterDefaultAllocator();
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
            s_jolt_registered = true;
            log_info_cat(AE_LOG_CATEGORY, "Jolt Physics: registered default allocator, factory, types");
        }
    }
}

// --- PhysicsWorld -------------------------------------------------------------

PhysicsWorld::PhysicsWorld() : impl_(std::make_unique<Impl>()) {
    ensure_jolt();
    log_info_cat(AE_LOG_CATEGORY, "PhysicsWorld created");
}

PhysicsWorld::~PhysicsWorld() {
    log_debug_cat(AE_LOG_CATEGORY, "PhysicsWorld destroyed");
}

BodyHandle PhysicsWorld::create_box_body(const BodyDesc& desc) {
    JPH::BoxShapeSettings shape_settings(JPH::Vec3(desc.half_extents.x, desc.half_extents.y, desc.half_extents.z));
    shape_settings.SetEmbedded();
    auto shape_result = shape_settings.Create();
    if (!shape_result.IsValid()) {
        log_error_cat(AE_LOG_CATEGORY, "create_box_body: invalid shape (half_extents=" +
                      std::to_string(desc.half_extents.x) + "," +
                      std::to_string(desc.half_extents.y) + "," +
                      std::to_string(desc.half_extents.z) + ")");
        return kInvalidBody;
    }

    JPH::EMotionType motion = desc.is_sensor ? JPH::EMotionType::Static :
        (desc.type == BodyType::Dynamic ? JPH::EMotionType::Dynamic :
         (desc.type == BodyType::Kinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Static));

    JPH::ObjectLayer layer = (motion == JPH::EMotionType::Static || desc.is_sensor)
        ? Layers::NON_MOVING : Layers::MOVING;

    JPH::BodyCreationSettings body_settings(
        shape_result.Get(),
        JPH::RVec3(desc.position.x, desc.position.y, desc.position.z),
        JPH::Quat::sIdentity(),
        motion,
        layer
    );

    auto& bi = impl_->physics_system.GetBodyInterface();
    JPH::BodyID jolt_id = bi.CreateAndAddBody(body_settings,
        motion == JPH::EMotionType::Dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);

    BodyHandle handle = static_cast<BodyHandle>(impl_->bodies.size());
    JoltBody jb;
    jb.jolt_id = jolt_id;
    jb.jolt_body = nullptr;
    jb.jump_through = false;
    impl_->bodies.push_back(jb);
    log_info_cat(AE_LOG_CATEGORY, "Created box body handle=" + std::to_string(handle) +
                  " type=" + std::to_string(static_cast<int>(desc.type)) +
                  " pos=(" + std::to_string(desc.position.x) + "," +
                  std::to_string(desc.position.y) + "," +
                  std::to_string(desc.position.z) + ")");
    return handle;
}

BodyHandle PhysicsWorld::create_capsule_body(const CapsuleDesc& desc, BodyType type) {
    float half_height = desc.half_height;
    float radius = desc.radius;
    if (half_height < 0.0F) {
        log_warning_cat(AE_LOG_CATEGORY, "create_capsule_body: negative half_height " +
                        std::to_string(half_height) + " clamped to 0");
        half_height = 0.0F;
    }

    JPH::CapsuleShapeSettings shape_settings(half_height, radius);
    shape_settings.SetEmbedded();
    auto shape_result = shape_settings.Create();
    if (!shape_result.IsValid()) {
        log_error_cat(AE_LOG_CATEGORY, "create_capsule_body: invalid shape (half_height=" +
                      std::to_string(half_height) + ", radius=" + std::to_string(radius) + ")");
        return kInvalidBody;
    }

    JPH::EMotionType motion = (type == BodyType::Dynamic) ? JPH::EMotionType::Dynamic :
                              (type == BodyType::Kinematic) ? JPH::EMotionType::Kinematic :
                              JPH::EMotionType::Static;

    JPH::ObjectLayer layer = (motion == JPH::EMotionType::Static) ? Layers::NON_MOVING : Layers::MOVING;

    JPH::BodyCreationSettings body_settings(
        shape_result.Get(),
        JPH::RVec3(desc.position.x, desc.position.y, desc.position.z),
        JPH::Quat::sIdentity(),
        motion,
        layer
    );

    auto& bi = impl_->physics_system.GetBodyInterface();
    JPH::BodyID jolt_id = bi.CreateAndAddBody(body_settings,
        motion == JPH::EMotionType::Dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);

    BodyHandle handle = static_cast<BodyHandle>(impl_->bodies.size());
    JoltBody jb;
    jb.jolt_id = jolt_id;
    jb.jolt_body = nullptr;
    jb.jump_through = false;
    impl_->bodies.push_back(jb);
    log_info_cat(AE_LOG_CATEGORY, "Created capsule body handle=" + std::to_string(handle) +
                  " half_height=" + std::to_string(half_height) + " radius=" + std::to_string(radius) +
                  " pos=(" + std::to_string(desc.position.x) + "," +
                  std::to_string(desc.position.y) + "," +
                  std::to_string(desc.position.z) + ")");
    return handle;
}

void PhysicsWorld::destroy_body(BodyHandle handle) {
    if (handle >= impl_->bodies.size()) {
        log_warning_cat(AE_LOG_CATEGORY, "destroy_body: invalid handle " + std::to_string(handle));
        return;
    }
    auto& body = impl_->bodies[handle];
    if (body.jolt_id.IsInvalid()) {
        log_debug_cat(AE_LOG_CATEGORY, "destroy_body: body " + std::to_string(handle) + " already destroyed");
        return;
    }
    auto& bi = impl_->physics_system.GetBodyInterface();
    bi.RemoveBody(body.jolt_id);
    bi.DestroyBody(body.jolt_id);
    body.jolt_id = JPH::BodyID();
    log_info_cat(AE_LOG_CATEGORY, "Destroyed body handle=" + std::to_string(handle));
}

void PhysicsWorld::set_body_position(BodyHandle handle, const Vec3& position) {
    if (handle >= impl_->bodies.size()) return;
    auto& body = impl_->bodies[handle];
    if (body.jolt_id.IsInvalid()) return;
    auto& bi = impl_->physics_system.GetBodyInterface();
    bi.SetPosition(body.jolt_id, JPH::RVec3(position.x, position.y, position.z), JPH::EActivation::DontActivate);
}

Vec3 PhysicsWorld::get_body_position(BodyHandle handle) const {
    if (handle >= impl_->bodies.size()) return {};
    auto& body = impl_->bodies[handle];
    if (body.jolt_id.IsInvalid()) return {};
    auto& bi = impl_->physics_system.GetBodyInterface();
    JPH::RVec3 pos = bi.GetPosition(body.jolt_id);
    return {pos.GetX(), pos.GetY(), pos.GetZ()};
}

void PhysicsWorld::set_body_velocity(BodyHandle handle, const Vec3& velocity) {
    if (handle >= impl_->bodies.size()) return;
    auto& body = impl_->bodies[handle];
    if (body.jolt_id.IsInvalid()) return;
    auto& bi = impl_->physics_system.GetBodyInterface();
    bi.SetLinearVelocity(body.jolt_id, JPH::Vec3(velocity.x, velocity.y, velocity.z));
}

CharacterHandle PhysicsWorld::create_character(const CharacterDesc& desc) {
    float radius = desc.radius;
    float half_height = (desc.height - 2.0F * radius) * 0.5F;
    if (half_height < 0.0F) half_height = 0.0F;

    JPH::RefConst<JPH::Shape> inner_shape = new JPH::CapsuleShape(half_height, radius);
    float offset_y = half_height + radius;
    JPH::RotatedTranslatedShapeSettings rt_settings(JPH::Vec3(0.0f, offset_y, 0.0f), JPH::Quat::sIdentity(), inner_shape);
    JPH::RefConst<JPH::Shape> shape = rt_settings.Create().Get();

    JPH::CharacterVirtualSettings char_settings;
    char_settings.mShape = shape;
    char_settings.mMaxSlopeAngle = JPH::DegreesToRadians(desc.max_slope_degrees);
    char_settings.mMass = desc.mass;
    char_settings.mMaxStrength = 100.0F;
    char_settings.mPredictiveContactDistance = 0.1F;
    char_settings.mCharacterPadding = 0.02F;

    auto* character = new JPH::CharacterVirtual(
        &char_settings,
        JPH::RVec3(desc.position.x, desc.position.y, desc.position.z),
        JPH::Quat::sIdentity(),
        &impl_->physics_system
    );
    character->SetListener(&impl_->contact_listener);

    CharacterHandle handle = static_cast<CharacterHandle>(impl_->characters.size());
    JoltCharacter jc;
    jc.standing_shape = shape;
    jc.character = character;
    jc.current_radius = radius;
    jc.current_height = desc.height;
    impl_->characters.push_back(jc);
    log_info_cat(AE_LOG_CATEGORY, "Created character handle=" + std::to_string(handle) +
                  " radius=" + std::to_string(radius) + " height=" + std::to_string(desc.height) +
                  " pos=(" + std::to_string(desc.position.x) + "," +
                  std::to_string(desc.position.y) + "," +
                  std::to_string(desc.position.z) + ")");
    return handle;
}

void PhysicsWorld::destroy_character(CharacterHandle handle) {
    if (handle >= impl_->characters.size()) {
        log_warning_cat(AE_LOG_CATEGORY, "destroy_character: invalid handle " + std::to_string(handle));
        return;
    }
    delete impl_->characters[handle].character;
    impl_->characters[handle].character = nullptr;
    log_info_cat(AE_LOG_CATEGORY, "Destroyed character handle=" + std::to_string(handle));
}

void PhysicsWorld::set_character_velocity(CharacterHandle handle, const Vec3& velocity) {
    if (handle >= impl_->characters.size()) return;
    auto* ch = impl_->characters[handle].character;
    if (!ch) return;
    ch->SetLinearVelocity(JPH::Vec3(velocity.x, velocity.y, velocity.z));
}

Vec3 PhysicsWorld::get_character_position(CharacterHandle handle) const {
    if (handle >= impl_->characters.size()) return {};
    auto* ch = impl_->characters[handle].character;
    if (!ch) return {};
    JPH::RVec3 pos = ch->GetPosition();
    return {pos.GetX(), pos.GetY(), pos.GetZ()};
}

Vec3 PhysicsWorld::get_character_velocity(CharacterHandle handle) const {
    if (handle >= impl_->characters.size()) return {};
    auto* ch = impl_->characters[handle].character;
    if (!ch) return {};
    JPH::Vec3 vel = ch->GetLinearVelocity();
    return {vel.GetX(), vel.GetY(), vel.GetZ()};
}

bool PhysicsWorld::character_on_ground(CharacterHandle handle) const {
    if (handle >= impl_->characters.size()) return false;
    auto* ch = impl_->characters[handle].character;
    if (!ch) return false;
    return ch->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
}

void PhysicsWorld::set_character_position(CharacterHandle handle, const Vec3& position) {
    if (handle >= impl_->characters.size()) return;
    auto* ch = impl_->characters[handle].character;
    if (!ch) return;
    ch->SetPosition(JPH::RVec3(position.x, position.y, position.z));
}

void PhysicsWorld::set_character_shape(CharacterHandle handle, float radius, float height) {
    if (handle >= impl_->characters.size()) return;
    auto& jc = impl_->characters[handle];
    if (!jc.character) return;

    float half_height = (height - 2.0F * radius) * 0.5F;
    if (half_height < 0.0F) half_height = 0.0F;

    JPH::RefConst<JPH::Shape> inner_shape = new JPH::CapsuleShape(half_height, radius);
    float offset_y = half_height + radius;
    JPH::RotatedTranslatedShapeSettings rt_settings(JPH::Vec3(0.0f, offset_y, 0.0f), JPH::Quat::sIdentity(), inner_shape);

    jc.character->SetShape(
        rt_settings.Create().Get(),
        1.5F,
        impl_->physics_system.GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
        impl_->physics_system.GetDefaultLayerFilter(Layers::MOVING),
        JPH::BodyFilter(),
        JPH::ShapeFilter(),
        impl_->temp_allocator
    );
    jc.current_radius = radius;
    jc.current_height = height;
    log_info_cat(AE_LOG_CATEGORY, "Character " + std::to_string(handle) + " shape changed: radius=" +
                  std::to_string(radius) + " height=" + std::to_string(height));
}

RayResult PhysicsWorld::raycast(const Vec3& origin, const Vec3& direction, float max_distance) const {
    RayResult result;
    JPH::Vec3 jorigin(origin.x, origin.y, origin.z);
    JPH::Vec3 jdir(direction.x, direction.y, direction.z);
    float len = std::sqrt(jdir.LengthSq());
    if (len < 0.0001F) {
        log_debug_cat(AE_LOG_CATEGORY, "raycast: direction vector too short (len=" + std::to_string(len) + ")");
        return result;
    }
    jdir = jdir / len;

    JPH::RRayCast ray {jorigin, jdir * max_distance};
    JPH::RayCastResult jresult;

    // Get filters from physics system
    auto bp_filter = impl_->physics_system.GetDefaultBroadPhaseLayerFilter(Layers::MOVING);
    auto ol_filter = impl_->physics_system.GetDefaultLayerFilter(Layers::MOVING);

    if (impl_->physics_system.GetNarrowPhaseQuery().CastRay(ray, jresult, bp_filter, ol_filter)) {
        result.hit = true;
        result.distance = jresult.mFraction * max_distance;
        result.point = {
            origin.x + direction.x * jresult.mFraction,
            origin.y + direction.y * jresult.mFraction,
            origin.z + direction.z * jresult.mFraction
        };
        result.body = static_cast<BodyHandle>(jresult.mBodyID.GetIndex());
    }
    return result;
}

void PhysicsWorld::set_body_jump_through(BodyHandle handle, bool enabled) {
    if (handle >= impl_->bodies.size()) {
        log_warning_cat(AE_LOG_CATEGORY, "set_body_jump_through: invalid handle " + std::to_string(handle));
        return;
    }
    impl_->bodies[handle].jump_through = enabled;
    log_debug_cat(AE_LOG_CATEGORY, "Body " + std::to_string(handle) + " jump_through = " + (enabled ? "true" : "false"));
}

void PhysicsWorld::tick(float delta_seconds) {
    log_trace_cat(AE_LOG_CATEGORY, "tick: dt=" + std::to_string(delta_seconds) +
                  " characters=" + std::to_string(impl_->characters.size()));
    for (auto& jc : impl_->characters) {
        if (!jc.character) continue;
        JPH::CharacterVirtual::ExtendedUpdateSettings update_settings;
        update_settings.mStickToFloorStepDown = JPH::Vec3(0.0f, -0.5f, 0.0f);
        update_settings.mWalkStairsStepUp = JPH::Vec3(0.0f, 0.4f, 0.0f);
        jc.character->ExtendedUpdate(
            delta_seconds,
            JPH::Vec3(0.0f, -20.0f, 0.0f),  // gravity
            update_settings,
            impl_->physics_system.GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
            impl_->physics_system.GetDefaultLayerFilter(Layers::MOVING),
            JPH::BodyFilter(),
            JPH::ShapeFilter(),
            impl_->temp_allocator
        );
    }
}

std::vector<PhysicsWorld::BodyDebugInfo> PhysicsWorld::get_body_debug_info() const {
    std::vector<BodyDebugInfo> infos;
    auto& bi = impl_->physics_system.GetBodyInterface();
    for (std::size_t i = 0; i < impl_->bodies.size(); ++i) {
        const auto& body = impl_->bodies[i];
        if (body.jolt_id.IsInvalid()) continue;
        JPH::RVec3 pos = bi.GetPosition(body.jolt_id);
        BodyDebugInfo info;
        info.handle = static_cast<BodyHandle>(i);
        info.position = {pos.GetX(), pos.GetY(), pos.GetZ()};
        info.half_extents = {0.5F, 0.5F, 0.5F};
        info.is_static = bi.GetMotionType(body.jolt_id) == JPH::EMotionType::Static;
        infos.push_back(info);
    }
    return infos;
}

}  // namespace ae::physics
