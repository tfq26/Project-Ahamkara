#pragma once

#include "ahamkara/game/net_types.h"
#include "ahamkara/game/world.h"

#include <memory>

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
    [[nodiscard]] float get_total_elapsed_seconds() const;
    [[nodiscard]] ae::u32 get_current_tick() const;
    [[nodiscard]] const ahamkara::game::ProjectileState* get_projectiles() const;
    [[nodiscard]] int get_projectile_count() const;
    [[nodiscard]] int get_ammo_current() const;
    [[nodiscard]] int get_ammo_max() const;

private:
    std::unique_ptr<IInputProvider> input_provider_;
    ahamkara::game::World world_;
    ae::u32 current_tick_ {0};
    ae::u32 sequence_ {0};
    float last_delta_seconds_ {0.0F};
    float total_elapsed_seconds_ {0.0F};
};

}  // namespace ahamkara::client
