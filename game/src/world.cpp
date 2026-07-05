#include "ahamkara/game/world.h"
#include "ahamkara/game/movement.h"
#include "ahamkara/game/game_module.h"
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

constexpr float kStandingVisualHeight = 0.65F;
constexpr float kCrouchingVisualHeight = 0.35F;
constexpr float kPlayerCollisionRadius = 0.22F;

}  // namespace

// --- World Implementation ----------------------------------------------------

World::World() : World(worlds::debug_javelin4()) {}

World::World(const WorldDefinition& definition) {
    initialize_jolt_once();

    player_.reset();
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
        JPH::RVec3(player_.state().position.x, player_.state().position.y, player_.state().position.z),
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
    hist.player_position = player_.state().position;
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
    player_.reset_to_spawn(player_spawn_);
    movement_controller_.reset_to_spawn(player_spawn_);
}

void World::tick(float delta_seconds, const PlayerInputCommand& input) {
    constexpr float kFixedStep = 1.0F / 60.0F;
    float time_remaining = delta_seconds;
    while (time_remaining > 0.0001F) {
        float step_dt = std::min(time_remaining, kFixedStep);
        advance_sim(step_dt);
        apply_input(step_dt, input);
        time_remaining -= step_dt;
    }
}

void World::advance_sim(float delta_seconds) {
    current_tick_++;

    match_time_ += delta_seconds;

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

    if (damage_feedback_timer_ > 0.0F) {
        damage_feedback_timer_ = std::max(0.0F, damage_feedback_timer_ - delta_seconds);
    }

    if (!is_client_) {
        tick_dummies(registry_, delta_seconds);
    }
    if (jolt_) {
        sync_dummies_to_jolt(jolt_->physics_system, jolt_->dummy_bodies, registry_);
    }

    if (!is_client_) {
        tick_dummy_ai(registry_, delta_seconds, player_.state().position, owned_colliders_, *this);
    }

    if (hitmarker_timer_ > 0.0F) {
        hitmarker_timer_ = std::max(0.0F, hitmarker_timer_ - delta_seconds);
    }
    if (muzzle_flash_timer_ > 0.0F) {
        muzzle_flash_timer_ = std::max(0.0F, muzzle_flash_timer_ - delta_seconds);
    }

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

    update_projectiles(delta_seconds);
    update_particles(delta_seconds);
    update_decals(delta_seconds);

    HistoricalState hist {};
    hist.tick = current_tick_;
    hist.player_position = player_.state().position;
    for (int idx = 0; idx < dummy_count_ && idx < HistoricalState::kMaxDummies; ++idx) {
        hist.dummy_positions[idx] = dummies_[idx].position;
        hist.dummy_alive[idx] = dummies_[idx].alive;
    }
    history_buffer_.push_back(hist);
    if (history_buffer_.size() > 120) {
        history_buffer_.pop_front();
    }

    flush_audio_events();
    sync_dummies_to_array();
    sync_projectiles_to_array();
}

void World::apply_input(float delta_seconds, const PlayerInputCommand& input) {
    // If the player is dead, skip input processing (respawn is handled in advance_sim).
    if (!is_player_alive()) return;

    const bool on_ground = is_on_ground();
    const JPH::Vec3 current_vel = jolt_->character->GetLinearVelocity();

    movement_controller_.begin_frame(
        player_.state(),
        input,
        delta_seconds,
        on_ground,
        {
            static_cast<float>(current_vel.GetX()),
            static_cast<float>(current_vel.GetY()),
            static_cast<float>(current_vel.GetZ())
        },
        cfg_walk_speed(),
        cfg_sprint_speed(),
        cfg_jump_speed(),
        cfg_gravity());

    bool want_crouch = movement_controller_.crouch_active();
    if (!want_crouch && jolt_->character->GetShape() == jolt_->crouching_shape) {
        bool allowed = jolt_->character->SetShape(
            jolt_->standing_shape,
            1.5f,
            jolt_->physics_system.GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
            jolt_->physics_system.GetDefaultLayerFilter(Layers::MOVING),
            JPH::BodyFilter(),
            JPH::ShapeFilter(),
            jolt_->temp_allocator);
        if (!allowed) {
            want_crouch = true;
        }
    } else if (want_crouch && jolt_->character->GetShape() == jolt_->standing_shape) {
        jolt_->character->SetShape(
            jolt_->crouching_shape,
            1.5f,
            jolt_->physics_system.GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
            jolt_->physics_system.GetDefaultLayerFilter(Layers::MOVING),
            JPH::BodyFilter(),
            JPH::ShapeFilter(),
            jolt_->temp_allocator);
    }

    const Vec3& desired_velocity = movement_controller_.desired_velocity();
    jolt_->character->SetLinearVelocity(JPH::Vec3(desired_velocity.x, desired_velocity.y, desired_velocity.z));

    JPH::CharacterVirtual::ExtendedUpdateSettings update_settings;
    update_settings.mStickToFloorStepDown = JPH::Vec3(0.0f, -0.35f, 0.0f);
    update_settings.mWalkStairsStepUp = JPH::Vec3(0.0f, 0.4f, 0.0f);

    jolt_->character->ExtendedUpdate(
        delta_seconds,
        JPH::Vec3(0.0f, -cfg_gravity(), 0.0f),
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
    player_.state().position.x = static_cast<float>(pos.GetX());
    player_.state().position.y = static_cast<float>(pos.GetY());
    player_.state().position.z = static_cast<float>(pos.GetZ());
    player_.state().velocity.x = static_cast<float>(vel.GetX());
    player_.state().velocity.y = static_cast<float>(vel.GetY());
    player_.state().velocity.z = static_cast<float>(vel.GetZ());

    // --- Slope slide ---
    bool on_ground_after = is_on_ground();
    if (on_ground_after && jolt_->character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround) {
        JPH::Vec3 jolt_gn = jolt_->character->GetGroundNormal();
        Vec3 ground_normal = {
            static_cast<float>(jolt_gn.GetX()),
            static_cast<float>(jolt_gn.GetY()),
            static_cast<float>(jolt_gn.GetZ())
        };
        float slope_deg = compute_slope_angle(ground_normal);
        constexpr MovementConfig kDefaultCfg;
        if (slope_deg > kDefaultCfg.max_walkable_slope) {
            Vec3 vel_slide = player_.state().velocity;
            apply_slope_physics(vel_slide, ground_normal, delta_seconds, kDefaultCfg);
            player_.state().velocity = vel_slide;
            jolt_->character->SetLinearVelocity(JPH::Vec3(vel_slide.x, vel_slide.y, vel_slide.z));
        }
    }

    // Ground floor clamp
    if (player_.state().position.y <= 0.0001F) {
        player_.state().position.y = 0.0F;
        if (player_.state().velocity.y < 0.0F) {
            player_.state().velocity.y = 0.0F;
        }
        jolt_->character->SetPosition(JPH::RVec3(player_.state().position.x, 0.0F, player_.state().position.z));
    }

    // Fall-death reset
    if (player_.state().position.y < -20.0F) {
        player_.state().position = { -12.0F, 2.0F, 0.0F };
        player_.state().velocity = {};
        jolt_->character->SetPosition(JPH::RVec3(player_.state().position.x, player_.state().position.y, player_.state().position.z));
        jolt_->character->SetLinearVelocity(JPH::Vec3(0, 0, 0));
    }

    movement_controller_.finish_frame(
        player_.state(),
        input,
        delta_seconds,
        is_on_ground(),
        colliders_,
        collider_count_,
        jolt_->character);

    if (input.reload_pressed) start_reload();
    if (input.weapon_slot != static_cast<ae::u8>(player_.loadout().active_slot) && input.weapon_slot < static_cast<ae::u8>(WeaponSlot::Count)) {
        switch_weapon(input.weapon_slot);
    }

    tick_weapon(delta_seconds, input.fire_held);

    if (input.fire_held) {
        const auto& def = get_active_weapon_def();
        if (def.fire_mode == FireMode::Hitscan || def.fire_mode == FireMode::Automatic) {
            fire_hitscan(*this, input);
        } else {
            spawn_projectile(input);
        }
    }
}


void World::set_player_state(const ReplicatedPlayerState& state) {
    player_.set_state(state);
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
    movement_controller_.finish_frame(player_.state(), PlayerInputCommand {}, 0.0F, is_on_ground(), colliders_, collider_count_, jolt_ ? jolt_->character : nullptr);
}

float World::get_player_visual_height() const {
    return movement_controller_.player_visual_height();
}

bool World::is_on_ground() const {
    if (jolt_ && jolt_->character) {
        if (jolt_->character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround) {
            return true;
        }
    }
    if (collider_count_ == 0 && player_.state().position.y <= 0.05F) {
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
        player_.state().position = {sp.pos_x, sp.pos_y, sp.pos_z};
        player_.state().yaw = sp.yaw;
        movement_controller_.reset_to_spawn({player_.state().position, sp.yaw});
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
    return player_.get_active_weapon_def();
}

void World::switch_weapon(int slot) {
    const int prev_slot = player_.loadout().active_slot;
    player_.switch_weapon(slot);
    if (player_.loadout().active_slot != prev_slot) {
        fire_recoil_index_ = 0;
    }
}

void World::start_reload() {
    player_.start_reload();
}

bool World::consume_ammo() {
    return player_.consume_ammo();
}

void World::tick_weapon(float delta_seconds, bool fire_held) {
    (void)fire_held;
    player_.tick_weapon(delta_seconds);
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
    auto view = registry_.view<const WorldProjectileComponent>();
    projectiles_.clear();
    projectiles_.reserve(view.size());
    for (auto entity : view) {
        projectiles_.push_back(view.get<const WorldProjectileComponent>(entity).state);
    }
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
        hist.player_position = player_.state().position;
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

    const float actual_damage = player_.apply_damage(damage);

    damage_feedback_timer_ = 0.3F;

    // Spawn damage number at player position
    Vec3 num_pos = player_.state().position;
    num_pos.y += 1.8F;
    spawn_damage_number(num_pos, actual_damage, false);

    // Hit audio
    queue_audio_event(AudioEvent{"player_hit", 1.0f, AudioCategory::SFX});

    if (!player_.is_alive()) {
        player_deaths_++;
        respawn_timer_ = 3.0F;
    }
}

void World::respawn_player() {
    reset_player_to_spawn();
    player_.reset_weapon_runtime(0, 150);

    if (jolt_ && jolt_->character) {
        jolt_->character->SetPosition(JPH::RVec3(
            player_.state().position.x,
            player_.state().position.y,
            player_.state().position.z
        ));
        jolt_->character->SetLinearVelocity(JPH::Vec3(0, 0, 0));
    }
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
    player_.reset_weapon_runtime(0, 150);
    if (jolt_ && jolt_->character) {
        jolt_->character->SetPosition(JPH::RVec3(
            player_.state().position.x,
            player_.state().position.y,
            player_.state().position.z
        ));
        jolt_->character->SetLinearVelocity(JPH::Vec3(0, 0, 0));
    }

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
