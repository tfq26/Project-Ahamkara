#pragma once

#include "ae/animation/character_weapon.h"
#include "ae/render/debug_renderer.h"
#include "ahamkara/client/debug_scene_bridge.h"

#include <array>

namespace ahamkara::client {

/// Client-side first-person weapon animation bridge.
///
/// The first production test targets the AR-15 because it is already the
/// primary weapon and has the clearest hold/fire/reload expectations. Other
/// weapons fall back to the legacy static presentation until they get their
/// own tuned profiles.
class WeaponAnimationController {
public:
    WeaponAnimationController();

    void reset();

    void tick(float dt,
              const ClientSimulationSnapshot& snapshot,
              const ahamkara::game::PlayerInputCommand& input);

    [[nodiscard]] bool has_transform() const { return has_transform_; }
    [[nodiscard]] const std::array<float, 16>& transform() const { return transform_; }

private:
    static constexpr int kArWeaponIndex = 0;
    static constexpr float kArReloadSeconds = 2.0F;  // must match WeaponRuntime::start_reload()

    static float horizontal_speed(const ahamkara::game::Vec3& velocity);
    static ae::render::Mat4 axis_angle_rotation(float x, float y, float z, float degrees);

    void update_ar15(float dt,
                     const ClientSimulationSnapshot& snapshot,
                     const ahamkara::game::PlayerInputCommand& input);

    int active_weapon_index_ {-1};
    bool has_transform_ {false};

    ae::animation::WeaponAnimConfig ar_config_ {};
    ae::animation::WeaponAnimState ar_state_ {};
    bool reload_active_ {false};
    float reload_timer_ {0.0F};
    std::array<float, 16> transform_ {};
};

}  // namespace ahamkara::client
