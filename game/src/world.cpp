#include "ahamkara/game/world.h"
#include "ahamkara/game/movement.h"
#include "ahamkara/game/worlds/debug_javelin4_world.h"
#include "ae/core/math.h"

#include "game_physics.h"
#include "world_dummy_sim.h"
#include "world_projectile.h"
#include "world_camera.h"

#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace ahamkara::game {
namespace {

constexpr float kGroundHeight = 0.0F;
constexpr float kJumpSpeed = 5.5F;
constexpr float kGravity = 18.0F;
constexpr float kWalkSpeed = 3.0F;
constexpr float kSprintSpeed = 6.0F;
constexpr float kSlideSpeed = 10.0F;
constexpr float kSlideDurationSeconds = 0.45F;
constexpr float kStandingEyeHeight = 0.58F;
constexpr float kCrouchingEyeHeight = 0.32F;
constexpr float kStandingVisualHeight = 0.65F;
constexpr float kCrouchingVisualHeight = 0.35F;
constexpr float kPlayerCollisionRadius = 0.22F;

// Quake/Source-style acceleration constants
constexpr float kGroundAccel = 12.0F;
constexpr float kAirAccel = 1.5F;
constexpr float kGroundFriction = 8.0F;
constexpr float kAirFriction = 0.5F;
constexpr float kMaxAirSpeed = 30.0F;
constexpr float kJumpBufferTime = 0.15F;
constexpr float kCoyoteTime = 0.10F;

}  // namespace

// --- World Implementation ----------------------------------------------------

World::World() : World(worlds::debug_javelin4()) {}

World::World(const WorldDefinition& definition) {
    initialize_jolt_once();

    reset_weapon_state();
    loadout_ = {};
    loadout_.weapons[static_cast<int>(WeaponSlot::Primary)] = 0;    // AR-15
    loadout_.weapons[static_cast<int>(WeaponSlot::Secondary)] = 1;  // Shotgun
    loadout_.weapons[static_cast<int>(WeaponSlot::Melee)] = 2;      // Rocket Launcher

    player_state_ = {};
    camera_anchor_ = {};
    apply_world_definition(definition);

    jolt_ = std::make_unique<GamePhysics>(this);

    // Setup standing shape: capsule centered at (0, offset_y, 0)
    float standing_radius = kPlayerCollisionRadius;
    float standing_height = kStandingVisualHeight;
    float standing_half_height = (standing_height - 2.0f * standing_radius) * 0.5f;
    if (standing_half_height < 0.0f) standing_half_height = 0.0f;

    JPH::RefConst<JPH::Shape> inner_standing = new JPH::CapsuleShape(standing_half_height, standing_radius);
    float standing_offset_y = standing_half_height + standing_radius;
    JPH::RotatedTranslatedShapeSettings standing_settings(JPH::Vec3(0.0f, standing_offset_y, 0.0f), JPH::Quat::sIdentity(), inner_standing);
    jolt_->standing_shape = standing_settings.Create().Get();

    // Setup crouching shape: capsule centered at (0, offset_y, 0)
    float crouching_radius = 0.15f;
    float crouching_height = kCrouchingVisualHeight;
    float crouching_half_height = (crouching_height - 2.0f * crouching_radius) * 0.5f;
    if (crouching_half_height < 0.0f) crouching_half_height = 0.0f;

    JPH::RefConst<JPH::Shape> inner_crouching = new JPH::CapsuleShape(crouching_half_height, crouching_radius);
    float crouching_offset_y = crouching_half_height + crouching_radius;
    JPH::RotatedTranslatedShapeSettings crouching_settings(JPH::Vec3(0.0f, crouching_offset_y, 0.0f), JPH::Quat::sIdentity(), inner_crouching);
    jolt_->crouching_shape = crouching_settings.Create().Get();

    // Initialize the kinematic character controller
    JPH::CharacterVirtualSettings char_settings;
    char_settings.mShape = jolt_->standing_shape;
    char_settings.mMaxSlopeAngle = JPH::DegreesToRadians(50.0f);
    char_settings.mMass = 70.0f;
    char_settings.mMaxStrength = 100.0f;
    char_settings.mPredictiveContactDistance = 0.1f;
    char_settings.mCharacterPadding = 0.02f;

    jolt_->character = new JPH::CharacterVirtual(
        &char_settings,
        JPH::RVec3(player_state_.position.x, player_state_.position.y, player_state_.position.z),
        JPH::Quat::sIdentity(),
        &jolt_->physics_system
    );
    jolt_->character->SetListener(&jolt_->contact_listener);

    for (int i = 0; i < dummy_count_; ++i) {
        auto entity = registry_.create();
        registry_.emplace<TargetDummyComponent>(entity, dummies_[i]);
    }

    // Initial colliders loading
    recreate_physics_colliders();

    // Save initial historical state
    HistoricalState hist {};
    hist.tick = 0;
    hist.player_position = player_state_.position;
    for (int i = 0; i < dummy_count_ && i < HistoricalState::kMaxDummies; ++i) {
        hist.dummy_positions[i] = dummies_[i].position;
        hist.dummy_alive[i] = dummies_[i].alive;
    }
    history_buffer_.push_back(hist);
}

World::~World() = default;

void World::apply_world_definition(const WorldDefinition& definition) {
    player_spawn_ = definition.player_spawn;
    reset_player_to_spawn();

    if (definition.map) {
        colliders_ = definition.map->colliders;
        collider_count_ = definition.map->collider_count;
    } else {
        colliders_ = nullptr;
        collider_count_ = 0;
    }

    const auto requested_count = definition.target_dummies ? definition.target_dummy_count : 0U;
    const auto source_count = std::min(requested_count, static_cast<std::size_t>(kMaxDummies));
    dummy_count_ = static_cast<int>(source_count);
    for (std::size_t idx = 0; idx < source_count; ++idx) {
        dummies_[idx] = definition.target_dummies[idx];
    }
    for (std::size_t idx = source_count; idx < static_cast<std::size_t>(kMaxDummies); ++idx) {
        dummies_[idx] = {};
        dummies_[idx].alive = false;
    }
}

void World::reset_player_to_spawn() {
    player_state_.position = player_spawn_.position;
    player_state_.velocity = {};
    player_state_.yaw = player_spawn_.yaw;
    player_state_.health = 100.0F;
    player_state_.shield = 100.0F;
    camera_anchor_.position = player_spawn_.position;
    camera_anchor_.yaw = player_spawn_.yaw;
    camera_anchor_.pitch = 0.0F;
}

void World::reset_weapon_state() {
    weapon_state_ = {};
    weapon_state_.definition_index = 0;
    weapon_state_.ammo_in_magazine = kWeaponRegistry[0].magazine_size;
    weapon_state_.magazine_capacity = kWeaponRegistry[0].magazine_size;
    weapon_state_.reserve_ammo = 150;
}

void World::tick(float delta_seconds, const PlayerInputCommand& input) {
    constexpr float kFixedStep = 1.0F / 60.0F;
    float time_remaining = delta_seconds;
    while (time_remaining > 0.0001F) {
        float step_dt = std::min(time_remaining, kFixedStep);
        tick_internal(step_dt, input);
        time_remaining -= step_dt;
    }
}

void World::tick_internal(float delta_seconds, const PlayerInputCommand& input) {
    current_tick_++;

    // Match time tracking
    match_time_ += delta_seconds;

    // --- Handle player respawn -----------------------------------------------
    if (!is_player_alive() && respawn_timer_ > 0.0F) {
        respawn_timer_ -= delta_seconds;
        if (respawn_timer_ <= 0.0F) {
            respawn_player();
            respawn_timer_ = 0.0F;
        }
    }
    if (!is_player_alive()) {
        flush_audio_events();
        return;
    }

    // --- Damage feedback timer -----------------------------------------------
    if (damage_feedback_timer_ > 0.0F) {
        damage_feedback_timer_ = std::max(0.0F, damage_feedback_timer_ - delta_seconds);
    }

    // --- Dummy simulation (extracted) -----------------------------------------
    if (!is_client_) {
        tick_dummies(registry_, delta_seconds);
    }
    if (jolt_) {
        sync_dummies_to_jolt(jolt_->physics_system, jolt_->dummy_bodies, registry_);
    }

    // --- Dummy AI (shoot at player) ------------------------------------------
    if (!is_client_) {
        tick_dummy_ai(registry_, delta_seconds, player_state_.position, owned_colliders_, *this);
    }

    // --- Feedback timers ------------------------------------------------------
    if (hitmarker_timer_ > 0.0F) {
        hitmarker_timer_ = std::max(0.0F, hitmarker_timer_ - delta_seconds);
    }
    if (muzzle_flash_timer_ > 0.0F) {
        muzzle_flash_timer_ = std::max(0.0F, muzzle_flash_timer_ - delta_seconds);
    }

    // --- Floating damage numbers ----------------------------------------------
    int active_numbers = 0;
    for (int idx = 0; idx < damage_number_count_; ++idx) {
        auto& dn = damage_numbers_[idx];
        dn.lifetime -= delta_seconds;
        if (dn.lifetime > 0.0F) {
            dn.position.y += 1.2F * delta_seconds;
            if (active_numbers != idx) {
                damage_numbers_[active_numbers] = dn;
            }
            active_numbers++;
        }
    }
    damage_number_count_ = active_numbers;

    // --- Slide / crouch -------------------------------------------------------
    slide_timer_seconds_ = std::max(0.0F, slide_timer_seconds_ - delta_seconds);
    if (input.slide_pressed && is_on_ground() && has_move_input(input)) {
        slide_timer_seconds_ = kSlideDurationSeconds;
    }

    crouch_active_ = input.crouch_held || slide_timer_seconds_ > 0.0F;

    // --- Stance transition checks ---------------------------------------------
    bool want_crouch = crouch_active_;
    if (!want_crouch && jolt_->character->GetShape() == jolt_->crouching_shape) {
        bool allowed = jolt_->character->SetShape(
            jolt_->standing_shape,
            1.5f,
            jolt_->physics_system.GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
            jolt_->physics_system.GetDefaultLayerFilter(Layers::MOVING),
            JPH::BodyFilter(),
            JPH::ShapeFilter(),
            jolt_->temp_allocator
        );
        if (!allowed) {
            crouch_active_ = true; // Stay crouched
        }
    } else if (want_crouch && jolt_->character->GetShape() == jolt_->standing_shape) {
        jolt_->character->SetShape(
            jolt_->crouching_shape,
            1.5f,
            jolt_->physics_system.GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
            jolt_->physics_system.GetDefaultLayerFilter(Layers::MOVING),
            JPH::BodyFilter(),
            JPH::ShapeFilter(),
            jolt_->temp_allocator
        );
    }

    // --- Movement acceleration (Quake-style) ----------------------------------
    float move_speed = kWalkSpeed;
    if (slide_timer_seconds_ > 0.0F) {
        move_speed = kSlideSpeed;
    } else if (input.sprint_held) {
        move_speed = kSprintSpeed;
    }

    const float yaw_rad = ae::to_radians(player_state_.yaw);
    const float forward_x = std::sin(yaw_rad);
    const float forward_z = std::cos(yaw_rad);
    const float right_x = std::cos(yaw_rad);
    const float right_z = -std::sin(yaw_rad);

    const float input_magnitude = std::min(
        std::sqrt(input.move_axis.x * input.move_axis.x + input.move_axis.y * input.move_axis.y),
        1.0F);
    const float wish_speed = move_speed * input_magnitude;

    bool on_ground = is_on_ground();
    JPH::Vec3 current_vel = jolt_->character->GetLinearVelocity();

    // --- Tick jump buffer timer ----------------------------------------------
    if (input.jump_pressed) {
        movement_sim_state_.jump_buffer_timer = kJumpBufferTime;
    } else if (movement_sim_state_.jump_buffer_timer > 0.0F) {
        movement_sim_state_.jump_buffer_timer =
            std::max(0.0F, movement_sim_state_.jump_buffer_timer - delta_seconds);
    }

    // --- Tick coyote timer ---------------------------------------------------
    if (!on_ground && movement_sim_state_.was_on_ground) {
        movement_sim_state_.coyote_timer = kCoyoteTime;
    } else if (movement_sim_state_.coyote_timer > 0.0F) {
        movement_sim_state_.coyote_timer =
            std::max(0.0F, movement_sim_state_.coyote_timer - delta_seconds);
    }

    // --- Resolve jump --------------------------------------------------------
    const bool can_jump = on_ground || (movement_sim_state_.coyote_timer > 0.0F);
    const bool want_jump = input.jump_pressed || (movement_sim_state_.jump_buffer_timer > 0.0F);

    float desired_vy = current_vel.GetY();

    if (want_jump && can_jump && slide_timer_seconds_ <= 0.0F) {
        desired_vy = kJumpSpeed;
        movement_sim_state_.jump_buffer_timer = 0.0F;
        movement_sim_state_.coyote_timer = 0.0F;
    } else if (!on_ground) {
        desired_vy -= kGravity * delta_seconds;
    } else {
        desired_vy = 0.0f;
    }

    // --- Horizontal acceleration / friction (Quake-style) --------------------
    float surf_speed_mult = surface_speed_multiplier(
        movement_sim_state_.ground_material, MovementConfig{});
    float surf_fric_mult = surface_friction_multiplier(
        movement_sim_state_.ground_material, MovementConfig{});

    float desired_vx = current_vel.GetX();
    float desired_vz = current_vel.GetZ();

    if (input_magnitude > 0.001F && slide_timer_seconds_ <= 0.0F) {
        const float inv_mag = 1.0F / input_magnitude;
        const float wish_x = input.move_axis.x * inv_mag;
        const float wish_y = input.move_axis.y * inv_mag;
        const float wish_dir_x = wish_x * right_x + wish_y * forward_x;
        const float wish_dir_z = wish_x * right_z + wish_y * forward_z;

        const float current_speed = desired_vx * wish_dir_x + desired_vz * wish_dir_z;
        const float add_speed = wish_speed * surf_speed_mult - current_speed;

        if (add_speed > 0.0F) {
            float accel_rate = on_ground ? kGroundAccel : kAirAccel;
            if (input.sprint_held && on_ground) accel_rate *= 1.0F;
            float accel_speed = accel_rate * delta_seconds * wish_speed * surf_speed_mult;
            if (accel_speed > add_speed) {
                accel_speed = add_speed;
            }
            desired_vx += accel_speed * wish_dir_x;
            desired_vz += accel_speed * wish_dir_z;
        }
    } else if (on_ground) {
        float h_speed = std::sqrt(desired_vx * desired_vx + desired_vz * desired_vz);
        if (h_speed > 0.001F) {
            float drop = kGroundFriction * surf_fric_mult * delta_seconds * h_speed;
            if (drop > h_speed) drop = h_speed;
            float scale = (h_speed - drop) / h_speed;
            desired_vx *= scale;
            desired_vz *= scale;
        }
    } else {
        float h_speed = std::sqrt(desired_vx * desired_vx + desired_vz * desired_vz);
        if (h_speed > 0.001F) {
            float drop = kAirFriction * delta_seconds * h_speed;
            if (drop > h_speed) drop = h_speed;
            float scale = (h_speed - drop) / h_speed;
            desired_vx *= scale;
            desired_vz *= scale;
        }
    }

    // --- Air speed cap -------------------------------------------------------
    if (!on_ground) {
        float total_speed = std::sqrt(
            desired_vx * desired_vx + desired_vy * desired_vy + desired_vz * desired_vz);
        if (total_speed > kMaxAirSpeed) {
            float scale = kMaxAirSpeed / total_speed;
            desired_vx *= scale;
            desired_vy *= scale;
            desired_vz *= scale;
        }
    }

    // Apply look delta & yaw
    player_state_.yaw += input.look_delta.x;

    // Persist ground state for next tick
    movement_sim_state_.was_on_ground = on_ground;

    // Capture pre-update vertical velocity for landing impulse
    float prev_vy = player_state_.velocity.y;

    // --- Resolve moving platform (area 37) ----------------------------------
    resolve_moving_platform(delta_seconds);

    // Apply motion to Jolt KCC
    jolt_->character->SetLinearVelocity(JPH::Vec3(desired_vx, desired_vy, desired_vz));

    JPH::CharacterVirtual::ExtendedUpdateSettings update_settings;
    update_settings.mStickToFloorStepDown = JPH::Vec3(0.0f, -0.5f, 0.0f);
    update_settings.mWalkStairsStepUp = JPH::Vec3(0.0f, 0.4f, 0.0f);

    jolt_->character->ExtendedUpdate(
        delta_seconds,
        JPH::Vec3(0.0f, -kGravity, 0.0f),
        update_settings,
        jolt_->physics_system.GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
        jolt_->physics_system.GetDefaultLayerFilter(Layers::MOVING),
        JPH::BodyFilter(),
        JPH::ShapeFilter(),
        jolt_->temp_allocator
    );

    // Sync player state back from KCC
    JPH::RVec3 pos = jolt_->character->GetPosition();
    JPH::Vec3 vel = jolt_->character->GetLinearVelocity();
    player_state_.position.x = static_cast<float>(pos.GetX());
    player_state_.position.y = static_cast<float>(pos.GetY());
    player_state_.position.z = static_cast<float>(pos.GetZ());
    player_state_.velocity.x = static_cast<float>(vel.GetX());
    player_state_.velocity.y = static_cast<float>(vel.GetY());
    player_state_.velocity.z = static_cast<float>(vel.GetZ());

    // Clamp to ground floor (0.0F)
    if (player_state_.position.y <= 0.0001F) {
        player_state_.position.y = 0.0F;
        if (player_state_.velocity.y < 0.0F) {
            player_state_.velocity.y = 0.0F;
        }
        jolt_->character->SetPosition(JPH::RVec3(player_state_.position.x, 0.0F, player_state_.position.z));
    }

    // Mantle detection
    resolve_mantle();

    // Ladder / Ledge detection (area 36)
    resolve_ladder_and_ledge(input);

    // Floor limit check
    if (player_state_.position.y < -20.0F) {
        player_state_.position = { -12.0F, 2.0F, 0.0F };
        player_state_.velocity = {};
        jolt_->character->SetPosition(JPH::RVec3(player_state_.position.x, player_state_.position.y, player_state_.position.z));
        jolt_->character->SetLinearVelocity(JPH::Vec3(0, 0, 0));
    }

    update_movement_state(input);
    update_camera(input, delta_seconds);

    // Landing impulse camera jolt
    if (is_on_ground() && !movement_sim_state_.was_on_ground && prev_vy < -0.01F) {
        Vec3 landing = compute_landing_impulse(std::abs(prev_vy), MovementConfig{});
        camera_anchor_.position.x += landing.x;
        camera_anchor_.position.y += landing.y;
    }

    populate_movement_debug(delta_seconds, input);

    // Handle reload input
    if (input.reload_pressed) start_reload();
    // Handle weapon switch from input
    if (input.weapon_slot != static_cast<ae::u8>(loadout_.active_slot) && input.weapon_slot < static_cast<ae::u8>(WeaponSlot::Count))
        switch_weapon(input.weapon_slot);

    // Weapon tick
    tick_weapon(delta_seconds, input.fire_held);

    if (input.fire_held) {
        const auto& def = get_active_weapon_def();
        if (def.fire_mode == FireMode::Hitscan || def.fire_mode == FireMode::Automatic)
            fire_hitscan(*this, input);
        else
            spawn_projectile(input);
    }

    update_projectiles(delta_seconds);
    update_particles(delta_seconds);
    update_decals(delta_seconds);

    // Save historical state for rollback lag compensation
    HistoricalState hist {};
    hist.tick = current_tick_;
    hist.player_position = player_state_.position;
    for (int idx = 0; idx < dummy_count_ && idx < HistoricalState::kMaxDummies; ++idx) {
        hist.dummy_positions[idx] = dummies_[idx].position;
        hist.dummy_alive[idx] = dummies_[idx].alive;
    }
    history_buffer_.push_back(hist);
    if (history_buffer_.size() > 120) {
        history_buffer_.pop_front();
    }

    flush_audio_events();

    // Centralized sync: EnTT registry → output arrays for renderer consumption.
    sync_dummies_to_array();
    sync_projectiles_to_array();
}

void World::set_player_state(const ReplicatedPlayerState& state) {
    player_state_ = state;
    if (jolt_ && jolt_->character) {
        jolt_->character->SetPosition(JPH::RVec3(state.position.x, state.position.y, state.position.z));
        jolt_->character->SetLinearVelocity(JPH::Vec3(state.velocity.x, state.velocity.y, state.velocity.z));
        jolt_->character->RefreshContacts(
            jolt_->physics_system.GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
            jolt_->physics_system.GetDefaultLayerFilter(Layers::MOVING),
            JPH::BodyFilter(),
            JPH::ShapeFilter(),
            jolt_->temp_allocator
        );
    }
    update_camera(PlayerInputCommand {}, 0.0F);
}

float World::get_player_visual_height() const {
    return crouch_active_ ? kCrouchingVisualHeight : kStandingVisualHeight;
}

bool World::is_on_ground() const {
    if (jolt_ && jolt_->character) {
        if (jolt_->character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround) {
            return true;
        }
    }
    if (collider_count_ == 0 && player_state_.position.y <= 0.05F) {
        return true;
    }
    return false;
}

void World::set_colliders(const ColliderBox* colliders, std::size_t count) {
    colliders_ = colliders;
    collider_count_ = count;
    recreate_physics_colliders();
}

bool World::load_colliders_from_level(const ae::render::LevelAsset& level) {
    if (level.collision_boxes.empty()) return false;
    std::vector<ColliderBox> boxes;
    boxes.reserve(level.collision_boxes.size());
    for (const auto& lcb : level.collision_boxes) {
        ColliderBox cb {};
        cb.min_x = lcb.min_x; cb.min_z = lcb.min_z;
        cb.max_x = lcb.max_x; cb.max_z = lcb.max_z;
        cb.top_y = lcb.top_y; cb.bottom_y = lcb.bottom_y;
        cb.wall = lcb.wall; cb.jump_through = lcb.jump_through;
        cb.auto_step = lcb.auto_step;
        cb.surface_material = static_cast<SurfaceMaterial>(
            std::min(lcb.surface_material, static_cast<std::uint32_t>(SurfaceMaterial::Count) - 1));
        boxes.push_back(cb);
    }
    if (!level.spawn_points.empty()) {
        const auto& sp = level.spawn_points[0];
        player_state_.position = {sp.pos_x, sp.pos_y, sp.pos_z};
        camera_anchor_.position = player_state_.position;
        player_state_.yaw = sp.yaw; camera_anchor_.yaw = sp.yaw;
        if (jolt_ && jolt_->character)
            jolt_->character->SetPosition(JPH::RVec3(sp.pos_x, sp.pos_y, sp.pos_z));
    }
    owned_colliders_ = std::move(boxes);
    colliders_ = owned_colliders_.data();
    collider_count_ = owned_colliders_.size();
    recreate_physics_colliders();
    return true;
}

const WeaponDefinition& World::get_active_weapon_def() const {
    const int idx = weapon_state_.definition_index;
    if (idx >= 0 && static_cast<std::size_t>(idx) < kWeaponRegistrySize)
        return kWeaponRegistry[idx];
    return kWeaponRegistry[0];
}

void World::switch_weapon(int slot) {
    const int idx = static_cast<int>(slot);
    if (idx < 0 || idx >= static_cast<int>(WeaponSlot::Count)) return;
    const int def_idx = loadout_.weapons[idx];
    if (def_idx < 0 || static_cast<std::size_t>(def_idx) >= kWeaponRegistrySize) return;
    if (def_idx == weapon_state_.definition_index) return;
    weapon_state_.definition_index = def_idx;
    const auto& def = kWeaponRegistry[def_idx];
    weapon_state_.magazine_capacity = def.magazine_size;
    weapon_state_.ammo_in_magazine = def.magazine_size;
    weapon_state_.reserve_ammo = def.magazine_size * 3;
    weapon_state_.fire_cooldown = 0.0F;
    weapon_state_.is_reloading = false;
    weapon_state_.is_equipping = false;
    fire_recoil_index_ = 0;
    loadout_.active_slot = idx;
}

void World::start_reload() {
    if (weapon_state_.can_reload()) {
        weapon_state_.is_reloading = true;
        reload_timer_ = 2.0F;
    }
}

bool World::consume_ammo() {
    if (weapon_state_.ammo_in_magazine <= 0) return false;
    weapon_state_.ammo_in_magazine--;
    return true;
}

void World::tick_weapon(float delta_seconds, bool fire_held) {
    (void)fire_held;
    if (weapon_state_.fire_cooldown > 0.0F)
        weapon_state_.fire_cooldown -= delta_seconds;
    if (weapon_state_.is_reloading) {
        reload_timer_ -= delta_seconds;
        if (reload_timer_ <= 0.0F) {
            const int needed = weapon_state_.magazine_capacity - weapon_state_.ammo_in_magazine;
            const int available = std::min(needed, weapon_state_.reserve_ammo);
            weapon_state_.ammo_in_magazine += available;
            weapon_state_.reserve_ammo -= available;
            weapon_state_.is_reloading = false;
            reload_timer_ = 0.0F;
        }
    }
    if (weapon_switch_queued_) {
        switch_weapon(queued_weapon_slot_);
        weapon_switch_queued_ = false;
        queued_weapon_slot_ = -1;
    }
}

void World::recreate_physics_colliders() {
    if (!jolt_) return;
    rebuild_jolt_colliders(*jolt_, colliders_, collider_count_, dummies_, dummy_count_, jolt_->standing_shape);
}

void World::resolve_mantle() {
    if (player_state_.velocity.y <= 0.5F) return;

    const float feet_y = player_state_.position.y;
    const float eye_y = feet_y + (crouch_active_ ? kCrouchingEyeHeight : kStandingEyeHeight);
    const float px = player_state_.position.x;
    const float pz = player_state_.position.z;
    constexpr float mantle_margin = 0.3F;

    for (std::size_t i = 0; i < collider_count_; ++i) {
        const auto& c = colliders_[i];
        if (c.wall || c.jump_through || !c.auto_step) continue;

        const bool in_x = px >= c.min_x - mantle_margin && px <= c.max_x + mantle_margin;
        const bool in_z = pz >= c.min_z - mantle_margin && pz <= c.max_z + mantle_margin;
        if (!in_x || !in_z) continue;

        if (eye_y < c.top_y) continue;

        const float dist_below = c.top_y - feet_y;
        if (dist_below < 0.2F || dist_below > 1.3F) continue;

        player_state_.position.y = c.top_y;
        player_state_.velocity.y = 0.0F;

        if (jolt_ && jolt_->character) {
            jolt_->character->SetPosition(JPH::RVec3(player_state_.position.x, player_state_.position.y, player_state_.position.z));
            jolt_->character->SetLinearVelocity(JPH::Vec3(player_state_.velocity.x, player_state_.velocity.y, player_state_.velocity.z));
        }
        return;
    }
}

void World::resolve_moving_platform(float delta_seconds) {
    (void)delta_seconds;
    if (!is_on_ground()) {
        movement_sim_state_.on_moving_platform = false;
        return;
    }

    JPH::BodyID ground_body_id = jolt_->character->GetGroundBodyID();
    if (ground_body_id.IsInvalid()) {
        movement_sim_state_.on_moving_platform = false;
        return;
    }

    auto& bi = jolt_->physics_system.GetBodyInterface();
    JPH::uint64 user_data = bi.GetUserData(ground_body_id);

    if (user_data >= collider_count_) {
        movement_sim_state_.on_moving_platform = false;
        return;
    }

    JPH::RVec3 current_pos = bi.GetPosition(ground_body_id);

    if (!movement_sim_state_.on_moving_platform) {
        movement_sim_state_.on_moving_platform = true;
        movement_sim_state_.platform_last_pos = {
            static_cast<float>(current_pos.GetX()),
            static_cast<float>(current_pos.GetY()),
            static_cast<float>(current_pos.GetZ())
        };
    } else {
        Vec3 delta = {
            static_cast<float>(current_pos.GetX()) - movement_sim_state_.platform_last_pos.x,
            static_cast<float>(current_pos.GetY()) - movement_sim_state_.platform_last_pos.y,
            static_cast<float>(current_pos.GetZ()) - movement_sim_state_.platform_last_pos.z
        };
        JPH::RVec3 char_pos = jolt_->character->GetPosition();
        jolt_->character->SetPosition(JPH::RVec3(
            char_pos.GetX() + delta.x,
            char_pos.GetY() + delta.y,
            char_pos.GetZ() + delta.z
        ));
        movement_sim_state_.platform_last_pos = {
            static_cast<float>(current_pos.GetX()),
            static_cast<float>(current_pos.GetY()),
            static_cast<float>(current_pos.GetZ())
        };
    }
}

void World::resolve_ladder_and_ledge(const PlayerInputCommand& input) {
    if (movement_sim_state_.ground_material == SurfaceMaterial::Ladder) {
        movement_sim_state_.on_ladder = true;
    } else if (movement_sim_state_.on_ladder && is_on_ground()) {
        movement_sim_state_.on_ladder = false;
    }
    if (movement_sim_state_.on_ladder) {
        float ladder_vy = input.move_axis.y * 4.0F;
        jolt_->character->SetLinearVelocity(JPH::Vec3(
            jolt_->character->GetLinearVelocity().GetX(),
            ladder_vy,
            jolt_->character->GetLinearVelocity().GetZ()
        ));
    }
    (void)input;
}

// --- Projectiles (delegated) --------------------------------------------------

void World::spawn_projectile(const PlayerInputCommand& input) {
    fire_projectile(*this, input);
}

void World::update_projectiles(float delta_seconds) {
    step_projectiles(*this, delta_seconds);
}

// --- Centralized array sync (EnTT → render arrays) ----------------------------
// OwnRSHIP: The EnTT registry is the authoritative runtime state for entities.
// These methods populate the output arrays from EnTT once per tick, so
// renderer consumers see a consistent snapshot without accessing EnTT directly.

void World::sync_dummies_to_array() {
    auto view = registry_.view<TargetDummyComponent>();
    int idx = 0;
    for (auto entity : view) {
        if (idx >= kMaxDummies) break;
        auto& comp = view.get<TargetDummyComponent>(entity);
        dummies_[idx] = comp.state;
        ++idx;
    }
    dummy_count_ = idx;
}

void World::sync_projectiles_to_array() {
    // Projectiles are managed directly via the array (not EnTT-backed yet).
    // This method exists as the documented sync boundary for when projectiles
    // migrate to EnTT.  Currently a no-op pass-through.
    // Future: auto view = registry_.view<ProjectileComponent>();
    //         iterate and populate projectiles_[] from EnTT.
}

// --- Camera / movement state (delegated) --------------------------------------

void World::update_camera(const PlayerInputCommand& input, float delta_seconds) {
    update_camera_state(camera_anchor_, player_state_, movement_sim_state_, delta_seconds, input, crouch_active_);
}

void World::update_movement_state(const PlayerInputCommand& input) {
    resolve_movement_state(player_state_, slide_timer_seconds_, movement_sim_state_, input, is_on_ground());
}

void World::populate_movement_debug(float delta_seconds, const PlayerInputCommand& input) {
    fill_movement_debug(movement_debug_, player_state_, movement_sim_state_, slide_timer_seconds_, delta_seconds, input, is_on_ground());
}

// --- Damage numbers -----------------------------------------------------------

void World::spawn_damage_number(const Vec3& position, float damage, bool is_critical) {
    if (damage_number_count_ < kMaxDamageNumbers) {
        auto& dn = damage_numbers_[damage_number_count_++];
        dn.position = position;
        dn.value = damage;
        dn.lifetime = 1.0F;
        dn.is_critical = is_critical;
    } else {
        int oldest = 0;
        float min_lifetime = damage_numbers_[0].lifetime;
        for (int i = 1; i < kMaxDamageNumbers; ++i) {
            if (damage_numbers_[i].lifetime < min_lifetime) {
                min_lifetime = damage_numbers_[i].lifetime;
                oldest = i;
            }
        }
        auto& dn = damage_numbers_[oldest];
        dn.position = position;
        dn.value = damage;
        dn.lifetime = 1.0F;
        dn.is_critical = is_critical;
    }
}

// --- Audio --------------------------------------------------------------------

void World::queue_audio_event(const AudioEvent& event) {
    if (audio_event_count_ < kMaxAudioEventsPerTick) {
        audio_event_queue_[audio_event_count_++] = event;
    }
}

void World::flush_audio_events() {
    if (audio_player_ && audio_event_count_ > 0) {
        for (int i = 0; i < audio_event_count_; ++i) {
            audio_player_->play_event(audio_event_queue_[i]);
        }
    }
    audio_event_count_ = 0;
}

// --- Historical state ---------------------------------------------------------

HistoricalState World::get_historical_state(ae::u32 target_tick) const {
    if (history_buffer_.empty()) {
        HistoricalState hist {};
        hist.tick = current_tick_;
        hist.player_position = player_state_.position;
        for (int i = 0; i < dummy_count_ && i < HistoricalState::kMaxDummies; ++i) {
            hist.dummy_positions[i] = dummies_[i].position;
            hist.dummy_alive[i] = dummies_[i].alive;
        }
        return hist;
    }

    float best_diff = 99999.0f;
    int best_idx = 0;
    for (int i = 0; i < (int)history_buffer_.size(); ++i) {
        float diff = std::abs((float)history_buffer_[i].tick - (float)target_tick);
        if (diff < best_diff) {
            best_diff = diff;
            best_idx = i;
        }
    }
    return history_buffer_[best_idx];
}

void World::spawn_muzzle_particles(const Vec3& position, const Vec3& forward) {
    for (int i = 0; i < 8; ++i) {
        if (particle_count_ >= kMaxParticles) break;
        auto& p = particles_[particle_count_++];
        p.position = position;
        float speed = 2.0F + static_cast<float>(std::rand() % 100) / 100.0F * 3.0F;
        p.velocity = {
            forward.x * speed + (static_cast<float>(std::rand() % 100) / 100.0F - 0.5F) * 0.5F,
            forward.y * speed + (static_cast<float>(std::rand() % 100) / 100.0F - 0.5F) * 0.5F,
            forward.z * speed + (static_cast<float>(std::rand() % 100) / 100.0F - 0.5F) * 0.5F
        };
        p.r = 1.0F; p.g = 0.8F; p.b = 0.3F;
        p.lifetime_seconds = 0.15F;
        p.max_lifetime = 0.15F;
        p.size = 0.04F;
        p.alive = true;
    }
}

void World::spawn_impact_particles(const Vec3& position, const Vec3& normal) {
    for (int i = 0; i < 12; ++i) {
        if (particle_count_ >= kMaxParticles) break;
        auto& p = particles_[particle_count_++];
        p.position = position;
        float speed = 1.0F + static_cast<float>(std::rand() % 100) / 100.0F * 2.0F;
        p.velocity = {
            normal.x * speed + (static_cast<float>(std::rand() % 100) / 100.0F - 0.5F) * 1.0F,
            normal.y * speed + (static_cast<float>(std::rand() % 100) / 100.0F - 0.5F) * 1.0F,
            normal.z * speed + (static_cast<float>(std::rand() % 100) / 100.0F - 0.5F) * 1.0F
        };
        p.r = 0.8F; p.g = 0.8F; p.b = 0.8F;
        p.lifetime_seconds = 0.3F;
        p.max_lifetime = 0.3F;
        p.size = 0.03F;
        p.alive = true;
    }
}

void World::spawn_bullet_hole_decal(const Vec3& position, const Vec3& normal) {
    if (decal_count_ >= kMaxDecals) {
        for (int i = 1; i < kMaxDecals; ++i) {
            decals_[i - 1] = decals_[i];
        }
        decal_count_ = kMaxDecals - 1;
    }
    auto& d = decals_[decal_count_++];
    d.position = position;
    d.normal = normal;
    d.size = 0.1F;
    d.r = 0.1F; d.g = 0.1F; d.b = 0.1F;
    d.lifetime_seconds = 10.0F;
    d.alive = true;
}

void World::update_particles(float delta_seconds) {
    for (int i = 0; i < particle_count_; ++i) {
        auto& p = particles_[i];
        if (!p.alive) continue;
        p.lifetime_seconds -= delta_seconds;
        if (p.lifetime_seconds <= 0.0F) {
            p.alive = false;
            continue;
        }
        p.position.x += p.velocity.x * delta_seconds;
        p.position.y += p.velocity.y * delta_seconds;
        p.position.z += p.velocity.z * delta_seconds;
    }
    int active = 0;
    for (int i = 0; i < particle_count_; ++i) {
        if (particles_[i].alive) {
            if (active != i) {
                particles_[active] = particles_[i];
            }
            active++;
        }
    }
    particle_count_ = active;
}

void World::update_decals(float delta_seconds) {
    for (int i = 0; i < decal_count_; ++i) {
        auto& d = decals_[i];
        if (!d.alive) continue;
        d.lifetime_seconds -= delta_seconds;
        if (d.lifetime_seconds <= 0.0F) {
            d.alive = false;
        }
    }
    int active = 0;
    for (int i = 0; i < decal_count_; ++i) {
        if (decals_[i].alive) {
            if (active != i) {
                decals_[active] = decals_[i];
            }
            active++;
        }
    }
    decal_count_ = active;
}

void World::apply_damage_to_player(float damage, const Vec3& attacker_pos) {
    if (!is_player_alive()) return;

    float actual_damage = damage;
    if (player_state_.shield > 0.0F) {
        constexpr float kArmorAbsorption = 0.66F;
        float armor_dmg = actual_damage * kArmorAbsorption;
        if (armor_dmg > player_state_.shield) {
            armor_dmg = player_state_.shield;
        }
        player_state_.shield -= armor_dmg;
        actual_damage = damage - armor_dmg;
    }
    player_state_.health -= actual_damage;
    if (player_state_.health < 0.0F) player_state_.health = 0.0F;

    damage_feedback_timer_ = 0.3F;

    // Spawn damage number at player position
    Vec3 num_pos = player_state_.position;
    num_pos.y += 1.8F;
    spawn_damage_number(num_pos, actual_damage, false);

    // Hit audio
    queue_audio_event(AudioEvent{"player_hit", 1.0f, AudioCategory::SFX});

    if (player_state_.health <= 0.0F) {
        player_deaths_++;
        respawn_timer_ = 3.0F;
    }
}

void World::respawn_player() {
    reset_player_to_spawn();

    if (jolt_ && jolt_->character) {
        jolt_->character->SetPosition(JPH::RVec3(
            player_state_.position.x,
            player_state_.position.y,
            player_state_.position.z
        ));
        jolt_->character->SetLinearVelocity(JPH::Vec3(0, 0, 0));
    }

    reset_weapon_state();
}

void World::on_dummy_killed(ae::u32 dummy_id, const Vec3& death_pos) {
    player_kills_++;

    Vec3 num_pos = death_pos;
    num_pos.y += 1.5F;
    spawn_damage_number(num_pos, 0.0F, false);

    // Check win condition: kill all dummies
    int alive_count = 0;
    for (int i = 0; i < dummy_count_; ++i) {
        if (dummies_[i].alive) alive_count++;
    }
    if (alive_count == 0 && player_kills_ >= 3) {
        match_over_ = true;
    }

    (void)dummy_id;
}

void World::restart_match() {
    player_kills_ = 0;
    player_deaths_ = 0;
    match_time_ = 0.0F;
    match_over_ = false;
    respawn_timer_ = 0.0F;
    damage_feedback_timer_ = 0.0F;

    reset_player_to_spawn();
    if (jolt_ && jolt_->character) {
        jolt_->character->SetPosition(JPH::RVec3(
            player_state_.position.x,
            player_state_.position.y,
            player_state_.position.z
        ));
        jolt_->character->SetLinearVelocity(JPH::Vec3(0, 0, 0));
    }

    reset_weapon_state();

    // Respawn all dummies
    auto view = registry_.view<TargetDummyComponent>();
    for (auto entity : view) {
        auto& comp = view.get<TargetDummyComponent>(entity);
        auto& d = comp.state;
        d.alive = true;
        d.health = 100.0F;
        d.armor = 50.0F;
        d.position = d.start_position;
        d.last_hit_timer = 0.0F;
        d.respawn_timer = 0.0F;
        comp.fire_timer = 1.0F + (static_cast<float>(std::rand() % 100) / 100.0F) * 2.0F;
        comp.burst_timer = 0.0F;
        comp.burst_count = 0;
    }
    sync_dummies_to_array();
}

ae::u8 World::get_match_phase() const {
    if (match_over_) return 6;
    if (respawn_timer_ > 0.0F) return 3;
    return 3;
}

}  // namespace ahamkara::game
