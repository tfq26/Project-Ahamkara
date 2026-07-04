#pragma once

#include "ahamkara/game/audio_events.h"
#include "ahamkara/game/camera_anchor.h"
#include "ahamkara/game/debug_map.h"
#include "ahamkara/game/movement.h"
#include "ahamkara/game/net_types.h"
#include "ahamkara/game/player.h"
#include "ahamkara/game/player_movement_controller.h"
#include "ahamkara/game/worlds/world_definition.h"
#include "ae/render/compiled_level.h"

#include <entt/entt.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <vector>
#include <string>

namespace ahamkara::game {

struct HistoricalState {
    ae::u32 tick {0};
    Vec3 player_position {};
    static constexpr int kMaxDummies = 4;
    Vec3 dummy_positions[kMaxDummies] {};
    bool dummy_alive[kMaxDummies] {};
};

struct FloatingDamageNumber {
    Vec3 position {};
    float value {0.0F};
    float lifetime {1.0F};
    bool is_critical {false};
};

class IAudioPlayer {
public:
    virtual ~IAudioPlayer() = default;
    virtual void play_sound(const std::string& name) = 0;
    virtual void play_event(const AudioEvent& event) = 0;
};

// Forward declaration for GamePhysics (defined in world_jolt_bridge.h)
struct JoltWorldImpl;
using GamePhysics = JoltWorldImpl;

class World {
    friend class AhamkaraCharacterContactListener;
public:
    World();
    explicit World(const WorldDefinition& definition);
    ~World();

    void tick(float delta_seconds, const PlayerInputCommand& input);

    [[nodiscard]] const ReplicatedPlayerState& get_player_state() const { return player_.state(); }
    [[nodiscard]] const CameraAnchor& get_camera_anchor() const { return movement_controller_.camera_anchor(); }
    [[nodiscard]] float get_player_visual_height() const;

    /// Projectiles live in the EnTT registry (the authoritative store).  The
    /// `projectiles_` member is a per-tick projection refreshed from the
    /// registry view via sync_projectiles_to_array(); these accessors keep
    /// their original pointer/count shape and are NOT a fixed-size array.
    [[nodiscard]] const ProjectileState* get_projectiles() const { return projectiles_.data(); }
    [[nodiscard]] int get_projectile_count() const { return static_cast<int>(projectiles_.size()); }
    [[nodiscard]] int get_max_projectiles() const { return kMaxProjectiles; }

    /// Mutable access to the dummy array for the simulation systems.
    /// Only simulation code (world_dummy_sim.cpp, world_projectile.cpp)
    /// should write through this.
    [[nodiscard]] TargetDummyState* dummies_mut() { return dummies_; }

    [[nodiscard]] int get_ammo_current() const { return player_.get_ammo_current(); }
    [[nodiscard]] int get_ammo_max() const { return player_.get_ammo_max(); }
    [[nodiscard]] int get_reserve_ammo() const { return player_.get_reserve_ammo(); }
    [[nodiscard]] int get_active_weapon_index() const { return player_.get_active_weapon_index(); }
    [[nodiscard]] const WeaponDefinition& get_active_weapon_def() const;
    [[nodiscard]] const WeaponState& get_weapon_state() const { return player_.get_weapon_state(); }

    void switch_weapon(int slot);
    void start_reload();
    bool consume_ammo();
    void tick_weapon(float delta_seconds, bool fire_held);
    [[nodiscard]] bool can_fire() const { return player_.can_fire(); }
    /// Notify the weapon runtime that a shot was fired.
    void notify_weapon_fired() { player_.notify_weapon_fired(); }

    [[nodiscard]] entt::registry& registry() { return registry_; }
    [[nodiscard]] const entt::registry& registry() const { return registry_; }

    void set_player_state(const ReplicatedPlayerState& state);
    void set_colliders(const ColliderBox* colliders, std::size_t count);

    /// Load collision boxes and world settings from a compiled level.
    /// LevelAsset is defined in ae::render::compiled_level.h (no GL deps).
    /// Returns false if the level asset is invalid.
    bool load_colliders_from_level(const ae::render::LevelAsset& level);

    static constexpr int kMaxDummies = 4;
    [[nodiscard]] const TargetDummyState* get_dummies() const { return dummies_; }
    [[nodiscard]] int get_dummy_count() const { return dummy_count_; }

    void set_is_client(bool is_client) { is_client_ = is_client; }
    [[nodiscard]] bool is_client() const { return is_client_; }
    void set_audio_player(IAudioPlayer* player) { audio_player_ = player; }

    static constexpr int kMaxDamageNumbers = 16;
    [[nodiscard]] const FloatingDamageNumber* get_damage_numbers() const { return damage_numbers_; }
    [[nodiscard]] int get_damage_number_count() const { return damage_number_count_; }

    [[nodiscard]] float get_hitmarker_time() const { return hitmarker_timer_; }
    [[nodiscard]] bool get_hitmarker_is_critical() const { return hitmarker_is_critical_; }
    [[nodiscard]] float get_muzzle_flash_time() const { return muzzle_flash_timer_; }

    void set_hitmarker(float time, bool is_critical) { hitmarker_timer_ = time; hitmarker_is_critical_ = is_critical; }
    void set_muzzle_flash(float time) { muzzle_flash_timer_ = time; }

    static constexpr int kMaxParticles = 256;
    [[nodiscard]] const ParticleState* get_particles() const { return particles_; }
    [[nodiscard]] int get_particle_count() const { return particle_count_; }

    static constexpr int kMaxDecals = 64;
    [[nodiscard]] const DecalState* get_decals() const { return decals_; }
    [[nodiscard]] int get_decal_count() const { return decal_count_; }

    void spawn_muzzle_particles(const Vec3& position, const Vec3& forward);
    void spawn_impact_particles(const Vec3& position, const Vec3& normal);
    void spawn_bullet_hole_decal(const Vec3& position, const Vec3& normal);

    /// Fire cooldown (seconds remaining).  Negative means ready to fire.
    /// Delegates to the active weapon state.
    [[nodiscard]] float fire_cooldown_timer() const { return player_.fire_cooldown_timer(); }
    void set_fire_cooldown_timer(float t) { player_.set_fire_cooldown_timer(t); }

    void spawn_damage_number(const Vec3& position, float damage, bool is_critical);
    void queue_audio_event(const AudioEvent& event);
    [[nodiscard]] HistoricalState get_historical_state(ae::u32 target_tick) const;

    void apply_damage_to_player(float damage, const Vec3& attacker_pos);
    [[nodiscard]] bool is_player_alive() const { return player_.is_alive(); }
    void respawn_player();
    [[nodiscard]] ae::u32 get_player_kills() const { return player_kills_; }
    [[nodiscard]] ae::u32 get_player_deaths() const { return player_deaths_; }
    [[nodiscard]] float get_match_time() const { return match_time_; }
    [[nodiscard]] ae::u8 get_match_phase() const;
    [[nodiscard]] ae::u32 get_team_score_red() const { return 0; }
    [[nodiscard]] ae::u32 get_team_score_blue() const { return player_kills_; }
    [[nodiscard]] bool is_match_over() const { return match_over_; }
    void restart_match();
    [[nodiscard]] const std::vector<ColliderBox>& get_world_colliders() const { return owned_colliders_; }

    void on_dummy_killed(ae::u32 dummy_id, const Vec3& death_pos);
    void set_is_server(bool is_server) { is_server_ = is_server; }
    [[nodiscard]] bool is_server() const { return is_server_; }

private:
    void apply_world_definition(const WorldDefinition& definition);
    void reset_player_to_spawn();
    void tick_internal(float delta_seconds, const PlayerInputCommand& input);
    void spawn_projectile(const PlayerInputCommand& input);
    void update_projectiles(float delta_seconds);
    void update_particles(float delta_seconds);
    void update_decals(float delta_seconds);
    void resolve_moving_platform(float delta_seconds);
    /// Centralized sync: EnTT registry → output arrays (called once per tick).
    /// The EnTT registry is the authoritative runtime state; output arrays
    /// are read-only render caches populated at tick end.
    void sync_dummies_to_array();
    void sync_projectiles_to_array();

    void recreate_physics_colliders();
    [[nodiscard]] bool is_on_ground() const;
    void flush_audio_events();

    static constexpr int kMaxAudioEventsPerTick = 16;

    Player player_;
    PlayerSpawnDefinition player_spawn_ {};
    PlayerMovementController movement_controller_;
    const ColliderBox* colliders_ {nullptr};
    std::size_t collider_count_ {0};
    std::vector<ColliderBox> owned_colliders_;
    static constexpr int kMaxProjectiles = 64;
    // Registry-backed projection of projectile state (refreshed per tick).
    // The EnTT registry is the authoritative store; this is a render cache.
    std::vector<ProjectileState> projectiles_;

    int fire_recoil_index_ {0};
    bool reload_key_was_down_ {false};
    bool weapon_switch_queued_ {false};
    int queued_weapon_slot_ {-1};

    TargetDummyState dummies_[kMaxDummies] {};
    int dummy_count_ {0};
    bool is_client_ {true};
    bool is_server_ {false};

    FloatingDamageNumber damage_numbers_[kMaxDamageNumbers] {};
    int damage_number_count_ {0};

    ParticleState particles_[kMaxParticles] {};
    int particle_count_ {0};

    DecalState decals_[kMaxDecals] {};
    int decal_count_ {0};

    float hitmarker_timer_ {0.0F};
    bool hitmarker_is_critical_ {false};
    float muzzle_flash_timer_ {0.0F};

    IAudioPlayer* audio_player_ {nullptr};

    std::unique_ptr<GamePhysics> jolt_;
    entt::registry registry_;
    ae::u32 current_tick_ {0};
    std::deque<HistoricalState> history_buffer_;
    AudioEvent audio_event_queue_[kMaxAudioEventsPerTick] {};
    int audio_event_count_ {0};

    float respawn_timer_ {0.0F};
    ae::u32 player_kills_ {0};
    ae::u32 player_deaths_ {0};
    float match_time_ {0.0F};
    bool match_over_ {false};
    float damage_feedback_timer_ {0.0F};
};

}  // namespace ahamkara::game
