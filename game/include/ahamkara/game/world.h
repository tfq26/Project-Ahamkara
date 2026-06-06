#pragma once

#include "ahamkara/game/debug_map.h"
#include "ahamkara/game/net_types.h"

#include <cstddef>

namespace ahamkara::game {

/**
 * @brief Represents a camera viewpoint in the world.
 * 
 * Separate from the player entity to allow for flexible camera logic
 * (e.g. third person, death cams, cinematics) while remaining grounded
 * in world coordinates.
 */
struct CameraAnchor {
    Vec3 position {};
    float yaw {0.0F};
    float pitch {0.0F};
};

/**
 * @brief The lightweight world runtime for Ahamkara.
 * 
 * Owns the high-level game state and provides a unified interface for
 * updating the simulation across client, server, and sandbox modes.
 */
class World {
public:
    World();

    /**
     * @brief Progresses the world simulation by one tick.
     * @param delta_seconds Time elapsed since the last tick.
     * @param input Input command for the local player.
     */
    void tick(float delta_seconds, const PlayerInputCommand& input);

    [[nodiscard]] const ReplicatedPlayerState& get_player_state() const { return player_state_; }
    [[nodiscard]] const CameraAnchor& get_camera_anchor() const { return camera_anchor_; }
    [[nodiscard]] float get_player_visual_height() const;

    /**
     * @brief Get active projectiles for rendering.
     */
    [[nodiscard]] const ProjectileState* get_projectiles() const { return projectiles_; }
    [[nodiscard]] int get_projectile_count() const { return projectile_count_; }
    [[nodiscard]] int get_max_projectiles() const { return kMaxProjectiles; }

    [[nodiscard]] int get_ammo_current() const { return ammo_current_; }
    [[nodiscard]] int get_ammo_max() const { return ammo_max_; }

    void set_player_state(const ReplicatedPlayerState& state);

    /**
     * @brief Replace the active collision set (e.g. for map changes).
     */
    void set_colliders(const ColliderBox* colliders, std::size_t count);

private:
    void resolve_platform_collisions();
    void resolve_horizontal_collisions();
    void resolve_mantle();
    void spawn_projectile();
    void update_projectiles(float delta_seconds);
    void update_camera(const PlayerInputCommand& input);
    void update_horizontal_motion(float delta_seconds, const PlayerInputCommand& input);
    void update_vertical_motion(float delta_seconds, const PlayerInputCommand& input);
    void update_movement_state(const PlayerInputCommand& input);
    [[nodiscard]] bool is_on_ground() const;

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
    float reload_timer_ {0.0F};
};

}  // namespace ahamkara::game
