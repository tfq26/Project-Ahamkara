#pragma once

#include "ahamkara/game/audio_events.h"
#include "ahamkara/game/debug_map.h"
#include "ahamkara/game/movement.h"
#include "ahamkara/game/net_types.h"

#include <entt/entt.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <string>

namespace ahamkara::game {

struct TargetDummyState {
    ae::u32 dummy_id {0};
    Vec3 position {};
    float yaw {0.0F};
    float health {100.0F};
    bool alive {true};
    Vec3 start_position {};
    Vec3 move_dir {};
    float move_timer {0.0F};
    float move_speed {0.0F};
    float move_distance {0.0F};
    float last_hit_timer {0.0F};
    bool was_hit_precision {false};
    float last_damage_dealt {0.0F};
    Vec3 last_hit_position {};
    float respawn_timer {0.0F};
};

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

struct CameraAnchor {
    Vec3 position {};
    float yaw {0.0F};
    float pitch {0.0F};
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
public:
    World();
    ~World();

    void tick(float delta_seconds, const PlayerInputCommand& input);

    [[nodiscard]] const ReplicatedPlayerState& get_player_state() const { return player_state_; }
    [[nodiscard]] const CameraAnchor& get_camera_anchor() const { return camera_anchor_; }
    [[nodiscard]] float get_player_visual_height() const;

    [[nodiscard]] const ProjectileState* get_projectiles() const { return projectiles_; }
    [[nodiscard]] int get_projectile_count() const { return projectile_count_; }
    [[nodiscard]] int get_max_projectiles() const { return kMaxProjectiles; }
    void set_projectile_count(int count) { projectile_count_ = count; }

    /// Mutable access to the projectile array for the fire/step systems.
    /// Only simulation code (world_projectile.cpp) should write through this.
    [[nodiscard]] ProjectileState* projectiles_mut() { return projectiles_; }

    /// Mutable access to the dummy array for the simulation systems.
    /// Only simulation code (world_dummy_sim.cpp, world_projectile.cpp)
    /// should write through this.
    [[nodiscard]] TargetDummyState* dummies_mut() { return dummies_; }

    [[nodiscard]] int get_ammo_current() const { return ammo_current_; }
    [[nodiscard]] int get_ammo_max() const { return ammo_max_; }

    void set_player_state(const ReplicatedPlayerState& state);
    void set_colliders(const ColliderBox* colliders, std::size_t count);

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
    /// Exposed for projectile system which needs to check and decrement.
    [[nodiscard]] float fire_cooldown_timer() const { return fire_cooldown_timer_; }
    void set_fire_cooldown_timer(float t) { fire_cooldown_timer_ = t; }

    void spawn_damage_number(const Vec3& position, float damage, bool is_critical);
    void queue_audio_event(const AudioEvent& event);

private:
    void tick_internal(float delta_seconds, const PlayerInputCommand& input);
    void update_movement_state(const PlayerInputCommand& input);
    void update_camera(const PlayerInputCommand& input, float delta_seconds);
    void spawn_projectile(const PlayerInputCommand& input);
    void update_projectiles(float delta_seconds);
    void update_particles(float delta_seconds);
    void update_decals(float delta_seconds);
    void resolve_mantle();
    void resolve_moving_platform(float delta_seconds);
    /// Centralized sync: EnTT registry → output arrays (called once per tick).
    /// The EnTT registry is the authoritative runtime state; output arrays
    /// are read-only render caches populated at tick end.
    void sync_dummies_to_array();
    void sync_projectiles_to_array();

    void resolve_ladder_and_ledge(const PlayerInputCommand& input);
    void recreate_physics_colliders();
    void populate_movement_debug(float delta_seconds, const PlayerInputCommand& input);
    [[nodiscard]] bool is_on_ground() const;
    [[nodiscard]] HistoricalState get_historical_state(ae::u32 target_tick) const;
    void flush_audio_events();

    static constexpr int kMaxAudioEventsPerTick = 16;

    ReplicatedPlayerState player_state_;
    CameraAnchor camera_anchor_;
    float slide_timer_seconds_ {0.0F};
    bool crouch_active_ {false};
    const ColliderBox* colliders_ {nullptr};
    std::size_t collider_count_ {0};
    static constexpr int kMaxProjectiles = 64;
    ProjectileState projectiles_[kMaxProjectiles] {};
    int projectile_count_ {0};

    int ammo_current_ {30};
    int ammo_max_ {30};

    TargetDummyState dummies_[kMaxDummies] {};
    int dummy_count_ {0};
    bool is_client_ {true};

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

    float fire_cooldown_timer_ {0.0F};

    std::unique_ptr<GamePhysics> jolt_;
    MovementSimState movement_sim_state_;
    MovementDebugState movement_debug_;
    entt::registry registry_;
    ae::u32 current_tick_ {0};
    std::vector<HistoricalState> history_buffer_;
    AudioEvent audio_event_queue_[kMaxAudioEventsPerTick] {};
    int audio_event_count_ {0};
};

}  // namespace ahamkara::game
