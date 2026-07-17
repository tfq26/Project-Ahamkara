#pragma once

#include "ae/collision/layers.h"
#include "ae/collision/types.h"
#include "ae/core/types.h"

#include <cstddef>
#include <memory>

namespace ae::collision {

class CollisionWorld;

// ============================================================
// CharacterDef — parameters for creating a character controller
// ============================================================

struct CharacterDef {
    Vec3 position {};
    float capsule_radius {0.22F};
    float capsule_half_height {0.3F}; // (standing_height - 2*radius) / 2
    float max_slope_angle_deg {50.0F};
    float mass {70.0F};
    float max_strength {100.0F};
    float predictive_contact_distance {0.1F};
    float character_padding {0.02F};
    CollisionLayer layer {GameLayers::PLAYER};
};

// ============================================================
// CharacterContactListener — virtual interface for contact
// validation callbacks from the character controller.
// ============================================================

class CharacterContactListener {
  public:
    virtual ~CharacterContactListener() = default;
    virtual bool on_contact_validate(u64 body_user_data) = 0;
};

// ============================================================
// CharacterController — wraps JPH::CharacterVirtual
//
// Created via CollisionWorld::create_character().
// All Jolt types remain behind the engine boundary.
// ============================================================

class CharacterController {
  public:
    ~CharacterController();

    CharacterController(const CharacterController&) = delete;
    CharacterController& operator=(const CharacterController&) = delete;
    CharacterController(CharacterController&&) = delete;
    CharacterController& operator=(CharacterController&&) = delete;

    // --- Position / velocity ---

    void set_position(const Vec3& pos);
    [[nodiscard]] Vec3 get_position() const;
    void set_linear_velocity(const Vec3& vel);
    [[nodiscard]] Vec3 get_linear_velocity() const;

    // --- Simulation ---

    /**
     * Extended character update: gravity, ground sticking, stair stepping,
     * and collision response against the owning CollisionWorld.
     */
    void extended_update(float delta_seconds, const Vec3& gravity,
                         float walk_stairs_step_up = 0.4F,
                         float stick_to_floor_step_down = 0.35F);

    /** Rebuild contact cache after teleport or manual position changes. */
    void refresh_contacts();

    // --- Ground state ---

    [[nodiscard]] bool is_on_ground() const;
    [[nodiscard]] Vec3 get_ground_normal() const;

    // --- Shape (crouching) ---

    /**
     * Set the character shape. The half_height and radius define a capsule.
     * Returns true if the shape change was allowed (no obstruction).
     */
    bool set_shape(float half_height, float radius);

    /** Opaque handle for comparing the current shape identity. */
    [[nodiscard]] const void* current_shape_id() const;

    // --- Listener ---

    void set_listener(CharacterContactListener* listener);

    /**
     * Phase 1 bridge: set the JPH::CharacterContactListener* directly.
     * Game code (AhamkaraCharacterContactListener) inherits from the
     * JPH interface; this allows registering it without exposing Jolt
     * types in the public API.  Will be replaced once the engine
     * interface fully abstracts contacts.
     */
    void set_jolt_contact_listener(void* jolt_listener);

    // --- Internal ---

    class Impl;
    [[nodiscard]] Impl* impl() {
        return impl_.get();
    }
    [[nodiscard]] const Impl* impl() const {
        return impl_.get();
    }

  private:
    friend class CollisionWorld;
    explicit CharacterController(CollisionWorld& world, const CharacterDef& def);
    std::unique_ptr<Impl> impl_;
};

} // namespace ae::collision
