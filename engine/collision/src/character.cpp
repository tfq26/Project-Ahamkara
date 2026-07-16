#include "ae/collision/character.h"
#include "jolt_backend.h"  // Internal: gives us CollisionWorld::Impl, jolt_helpers, helpers

#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

#define AE_LOG_CATEGORY "Collision"

namespace ae::collision {

// ============================================================
// CharacterController::Impl
// ============================================================

class CharacterController::Impl {
public:
    CollisionWorld* world_;
    JPH::CharacterVirtual* character_ {nullptr};

    // Standalone shapes cached for crouch/stand transitions.
    // These are NOT the Jolt shapes used by the character — Jolt
    // owns its own via CharacterVirtualSettings.  We keep a ref
    // here so set_shape() can produce a shifted-capsule that
    // Jolt can use.
    JPH::RefConst<JPH::Shape> standing_shape_;
    JPH::RefConst<JPH::Shape> crouching_shape_;

    CharacterDef def_;

    Impl(CollisionWorld& world, const CharacterDef& def)
        : world_(&world),
          def_(def)
    {
        auto* phys = world_->impl()->physics();

        // Build the standing capsule shape (centered above origin).
        float standing_offset_y = def_.capsule_half_height + def_.capsule_radius;
        JPH::RefConst<JPH::Shape> inner_standing = new JPH::CapsuleShape(def_.capsule_half_height, def_.capsule_radius);
        JPH::RotatedTranslatedShapeSettings standing_settings(
            JPH::Vec3(0.0F, standing_offset_y, 0.0F),
            JPH::Quat::sIdentity(),
            inner_standing);
        standing_shape_ = standing_settings.Create().Get();

        // Crouching shape — same geometry initially, game may change later.
        crouching_shape_ = standing_shape_;

        // CharacterVirtual settings
        JPH::CharacterVirtualSettings char_settings;
        char_settings.mShape = standing_shape_;
        char_settings.mMaxSlopeAngle = JPH::DegreesToRadians(def_.max_slope_angle_deg);
        char_settings.mMass = def_.mass;
        char_settings.mMaxStrength = def_.max_strength;
        char_settings.mPredictiveContactDistance = def_.predictive_contact_distance;
        char_settings.mCharacterPadding = def_.character_padding;

        character_ = new JPH::CharacterVirtual(
            &char_settings,
            to_jolt_rvec3(def_.position),
            JPH::Quat::sIdentity(),
            phys);
    }

    ~Impl() {
        if (character_) {
            delete character_;
            character_ = nullptr;
        }
    }

    [[nodiscard]] JPH::PhysicsSystem& physics() {
        return *world_->impl()->physics();
    }
};

// ============================================================
// CharacterController public API
// ============================================================

CharacterController::CharacterController(CollisionWorld& world, const CharacterDef& def)
    : impl_(std::make_unique<Impl>(world, def)) {}

CharacterController::~CharacterController() = default;

void CharacterController::set_position(const Vec3& pos) {
    impl_->character_->SetPosition(to_jolt_rvec3(pos));
}

Vec3 CharacterController::get_position() const {
    return from_jolt_rvec3(impl_->character_->GetPosition());
}

void CharacterController::set_linear_velocity(const Vec3& vel) {
    impl_->character_->SetLinearVelocity(to_jolt_vec3(vel));
}

Vec3 CharacterController::get_linear_velocity() const {
    return from_jolt_vec3(impl_->character_->GetLinearVelocity());
}

void CharacterController::extended_update(
    float delta_seconds,
    const Vec3& gravity,
    float walk_stairs_step_up,
    float stick_to_floor_step_down)
{
    auto& phys = impl_->physics();

    JPH::CharacterVirtual::ExtendedUpdateSettings update_settings;
    update_settings.mStickToFloorStepDown = JPH::Vec3(0.0F, -stick_to_floor_step_down, 0.0F);
    update_settings.mWalkStairsStepUp = JPH::Vec3(0.0F, walk_stairs_step_up, 0.0F);

    impl_->character_->ExtendedUpdate(
        delta_seconds,
        to_jolt_vec3(gravity),
        update_settings,
        phys.GetDefaultBroadPhaseLayerFilter(jolt_helpers::kJoltLayerPlayer),
        phys.GetDefaultLayerFilter(jolt_helpers::kJoltLayerPlayer),
        JPH::BodyFilter(),
        JPH::ShapeFilter(),
        *impl_->world_->impl()->temp_allocator_ptr());
}

void CharacterController::refresh_contacts() {
    auto& phys = impl_->physics();
    impl_->character_->RefreshContacts(
        phys.GetDefaultBroadPhaseLayerFilter(jolt_helpers::kJoltLayerPlayer),
        phys.GetDefaultLayerFilter(jolt_helpers::kJoltLayerPlayer),
        JPH::BodyFilter(),
        JPH::ShapeFilter(),
        *impl_->world_->impl()->temp_allocator_ptr());
}

bool CharacterController::is_on_ground() const {
    return impl_->character_->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
}

Vec3 CharacterController::get_ground_normal() const {
    return from_jolt_vec3(impl_->character_->GetGroundNormal());
}

bool CharacterController::set_shape(float half_height, float radius) {
    auto& phys = impl_->physics();
    JPH::RefConst<JPH::Shape> inner = new JPH::CapsuleShape(half_height, radius);
    float offset_y = half_height + radius;
    JPH::RotatedTranslatedShapeSettings rts(JPH::Vec3(0.0F, offset_y, 0.0F), JPH::Quat::sIdentity(), inner);
    JPH::ShapeSettings::ShapeResult result = rts.Create();
    if (!result.IsValid()) return false;
    JPH::RefConst<JPH::Shape> new_shape = result.Get();
    bool allowed = impl_->character_->SetShape(
        new_shape,
        1.5F,
        phys.GetDefaultBroadPhaseLayerFilter(jolt_helpers::kJoltLayerPlayer),
        phys.GetDefaultLayerFilter(jolt_helpers::kJoltLayerPlayer),
        JPH::BodyFilter(),
        JPH::ShapeFilter(),
        *impl_->world_->impl()->temp_allocator_ptr());
    if (allowed) {
        // Cache the shape for current_shape_id() comparisons.
        impl_->standing_shape_ = new_shape;
    }
    return allowed;
}

const void* CharacterController::current_shape_id() const {
    return impl_->character_->GetShape();
}

void CharacterController::set_listener(CharacterContactListener* listener) {
    // We need a JPH::CharacterContactListener to bridge our virtual interface.
    // Since the game's contact listener (AhamkaraCharacterContactListener) is
    // defined in game code and inherits JPH::CharacterContactListener directly,
    // we accept it at the JPH level.  The public API uses the engine-agnostic
    // CharacterContactListener adapter — but for Phase 1 the game continues to
    // set its JPH listener directly via set_jolt_contact_listener().
    // (This is intentionally left for the bridge code to manage.)
    (void)listener;
}

void CharacterController::set_jolt_contact_listener(void* jolt_listener) {
    impl_->character_->SetListener(
        static_cast<JPH::CharacterContactListener*>(jolt_listener));
}

}  // namespace ae::collision
