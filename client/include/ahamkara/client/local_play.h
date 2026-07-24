#pragma once

#include "ahamkara/game/net_types.h"
#include "ahamkara/game/world.h"
#include "ae/core/tick.h"

#include <memory>
#include <string>

namespace ahamkara::client {

/**
 * @brief Interface for providing player inputs to the movement simulation.
 * Decouples input gathering from simulation so local inputs can be swapped for network inputs.
 */
class IInputProvider {
public:
    virtual ~IInputProvider() = default;

    /**
     * @brief Gather input command for the current frame.
     * @param delta_seconds Frame delta time.
     */
    virtual ahamkara::game::PlayerInputCommand gather_input(float delta_seconds) = 0;
    [[nodiscard]] virtual bool finished() const { return false; }
};

/**
 * @brief Manages the single-player simulation loop locally.
 * Exposes player state, camera state, and timing cleanly.
 */
class LocalPlaySimulation {
public:
    explicit LocalPlaySimulation(std::unique_ptr<IInputProvider> input_provider);

    /**
     * @brief Step the local simulation forward.
     * @param delta_seconds Time elapsed since the last frame.
     */
    void tick(float delta_seconds);

    [[nodiscard]] const ahamkara::game::ReplicatedPlayerState& get_player_state() const;
    [[nodiscard]] const ahamkara::game::CameraAnchor& get_camera_anchor() const;
    [[nodiscard]] float get_player_visual_height() const;
    [[nodiscard]] float get_last_delta_seconds() const;
    [[nodiscard]] float get_last_frame_delta_seconds() const;
    [[nodiscard]] float get_total_elapsed_seconds() const;
    [[nodiscard]] ae::u32 get_current_tick() const;
    [[nodiscard]] float get_interpolation_alpha() const;
    [[nodiscard]] double get_fixed_step_seconds() const;
    [[nodiscard]] const ahamkara::game::ProjectileState* get_projectiles() const;
    [[nodiscard]] int get_projectile_count() const;
    [[nodiscard]] int get_ammo_current() const;
    [[nodiscard]] int get_ammo_max() const;
    [[nodiscard]] int get_reserve_ammo() const;
    [[nodiscard]] int get_active_weapon_index() const;
    [[nodiscard]] bool get_is_reloading() const;

    [[nodiscard]] const ahamkara::game::TargetDummyState* get_dummies() const;
    [[nodiscard]] int get_dummy_count() const;
    [[nodiscard]] const ahamkara::game::FloatingDamageNumber* get_damage_numbers() const;
    [[nodiscard]] int get_damage_number_count() const;
    [[nodiscard]] float get_hitmarker_time() const;
    [[nodiscard]] bool get_hitmarker_is_critical() const;
    [[nodiscard]] float get_muzzle_flash_time() const;
    [[nodiscard]] int get_interaction_attempt_count() const;
    [[nodiscard]] int get_interaction_success_count() const;
    [[nodiscard]] bool did_last_interaction_succeed() const;
    [[nodiscard]] const std::string& get_last_interaction_label() const;
    [[nodiscard]] int get_reload_request_count() const;
    [[nodiscard]] int get_ability_use_count() const;

    [[nodiscard]] const ahamkara::game::ParticleState* get_particles() const;
    [[nodiscard]] int get_particle_count() const;
    [[nodiscard]] const ahamkara::game::DecalState* get_decals() const;
    [[nodiscard]] int get_decal_count() const;

    [[nodiscard]] ahamkara::game::Vec3 get_interpolated_player_position(float alpha) const;
    [[nodiscard]] ahamkara::game::CameraAnchor get_interpolated_camera_anchor(float alpha) const;
    [[nodiscard]] ahamkara::game::TargetDummyState get_interpolated_dummy(int idx, float alpha) const;

    void set_colliders(const ahamkara::game::ColliderBox* colliders, std::size_t count);
    void set_interaction_targets(const ahamkara::game::InteractionTargetDefinition* targets, std::size_t count);
    void set_audio_player(ahamkara::game::IAudioPlayer* player);
    bool load_level(const std::string& path);

    [[nodiscard]] ae::u32 get_player_kills() const;
    [[nodiscard]] ae::u32 get_player_deaths() const;
    [[nodiscard]] float get_match_time() const;
    [[nodiscard]] ae::u8 get_match_phase() const;
    [[nodiscard]] bool is_match_over() const;
    [[nodiscard]] bool is_player_alive() const;
    [[nodiscard]] float get_damage_feedback_timer() const;
    [[nodiscard]] const ahamkara::game::AbilityState& get_ability_state() const;
    void restart_match();

private:
    std::unique_ptr<IInputProvider> input_provider_;
    ahamkara::game::World world_;
    ae::u32 current_tick_ {0};
    ae::u32 sequence_ {0};
    float last_delta_seconds_ {0.0F};
    float last_frame_delta_seconds_ {0.0F};
    float total_elapsed_seconds_ {0.0F};
    ae::FixedTimestepAccumulator fixed_timestep_ {1.0 / 60.0};

    // Interpolation states
    ahamkara::game::ReplicatedPlayerState previous_player_state_;
    ahamkara::game::CameraAnchor previous_camera_anchor_;
    ahamkara::game::TargetDummyState previous_dummies_[ahamkara::game::World::kMaxDummies] {};
    bool has_previous_state_ {false};
};

}  // namespace ahamkara::client
