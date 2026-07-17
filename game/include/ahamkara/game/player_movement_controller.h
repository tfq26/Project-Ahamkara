#pragma once

#include "ahamkara/game/camera_anchor.h"
#include "ahamkara/game/debug_map.h"
#include "ahamkara/game/movement.h"
#include "ahamkara/game/net_types.h"
#include "ahamkara/game/worlds/world_definition.h"

#include <cstddef>

namespace ae::collision {
class CharacterController;
}

namespace ahamkara::game {

class PlayerMovementController {
public:
    PlayerMovementController() = default;

    void reset_to_spawn(const PlayerSpawnDefinition& spawn);

    void begin_frame(
        ReplicatedPlayerState& player_state,
        const PlayerInputCommand& input,
        float delta_seconds,
        bool on_ground,
        const Vec3& current_velocity,
        float walk_speed,
        float sprint_speed,
        float jump_speed,
        float gravity);

    void finish_frame(
        ReplicatedPlayerState& player_state,
        const PlayerInputCommand& input,
        float delta_seconds,
        bool on_ground,
        const ColliderBox* colliders,
        std::size_t collider_count,
        ae::collision::CharacterController* character);

    [[nodiscard]] const CameraAnchor& camera_anchor() const { return camera_anchor_; }
    [[nodiscard]] const MovementDebugState& movement_debug() const { return movement_debug_; }
    [[nodiscard]] bool crouch_active() const { return crouch_active_; }
    [[nodiscard]] float player_visual_height() const { return crouch_active_ ? kCrouchingVisualHeight : kStandingVisualHeight; }
    [[nodiscard]] const Vec3& desired_velocity() const { return desired_velocity_; }

private:
    static constexpr float kStandingEyeHeight = 0.58F;
    static constexpr float kCrouchingEyeHeight = 0.32F;
    static constexpr float kStandingVisualHeight = 0.65F;
    static constexpr float kCrouchingVisualHeight = 0.35F;

    void resolve_mantle(
        ReplicatedPlayerState& player_state,
        const ColliderBox* colliders,
        std::size_t collider_count,
        ae::collision::CharacterController* character);

    void resolve_ladder_and_ledge(
        ReplicatedPlayerState& player_state,
        const PlayerInputCommand& input,
        ae::collision::CharacterController* character);

    void update_camera_and_debug(
        ReplicatedPlayerState& player_state,
        const PlayerInputCommand& input,
        float delta_seconds,
        bool on_ground);

    CameraAnchor camera_anchor_ {};
    MovementSimState movement_sim_state_ {};
    MovementDebugState movement_debug_ {};
    Vec3 desired_velocity_ {};
    float slide_timer_seconds_ {0.0F};
    bool crouch_active_ {false};
    bool on_ground_before_step_ {false};
    float previous_vertical_velocity_ {0.0F};
};

}  // namespace ahamkara::game
