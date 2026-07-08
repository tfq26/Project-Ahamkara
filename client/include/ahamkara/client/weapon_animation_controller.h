#pragma once

#include "ae/animation/character_weapon.h"
#include "ae/render/debug_renderer.h"
#include "ahamkara/client/debug_scene_bridge.h"
#include "ahamkara/client/weapon_viewmodel_data.h"

#include <array>
#include <unordered_map>

namespace ahamkara::client {

/// Per-weapon animation profile with tuned parameters.
struct WeaponAnimProfile {
    ae::animation::WeaponAnimConfig anim_config {};
    float reload_duration {2.0F};
    float melee_duration {0.6F};
    float melee_reach {1.5F};
    float melee_damage {35.0F};
    WeaponReloadData reload_data {};
};

/// Client-side first-person weapon animation bridge.
///
/// Supports multiple weapon profiles via an unordered_map keyed by weapon index.
/// Falls back to the AR-15 profile for unregistered weapons.
class WeaponAnimationController {
public:
    WeaponAnimationController();

    void reset();

    void tick(float dt,
              const ClientSimulationSnapshot& snapshot,
              const ahamkara::game::PlayerInputCommand& input);

    [[nodiscard]] bool has_transform() const { return has_transform_; }
    [[nodiscard]] const std::array<float, 16>& transform() const { return transform_; }
    [[nodiscard]] float ads_blend() const { return anim_state_.ads_blend; }

    // Melee API
    bool trigger_melee();
    [[nodiscard]] bool is_melee_active() const { return melee_active_; }
    [[nodiscard]] float melee_normalized() const;

    // Reload phase API
    [[nodiscard]] ReloadPhase reload_phase() const { return reload_phase_; }
    [[nodiscard]] const float* reload_ik_offset() const { return reload_ik_offset_; }
    [[nodiscard]] float reload_normalized() const { return reload_normalized_; }

private:
    static constexpr int kArWeaponIndex = 0;
    static constexpr int kShotgunWeaponIndex = 1;
    static constexpr int kRlWeaponIndex = 2;

    static float horizontal_speed(const ahamkara::game::Vec3& velocity);

    static WeaponAnimProfile make_ar15_profile();
    static WeaponAnimProfile make_shotgun_profile();
    static WeaponAnimProfile make_rl_profile();

    void update_weapon(float dt,
                       const ClientSimulationSnapshot& snapshot,
                       const ahamkara::game::PlayerInputCommand& input);

    int active_weapon_index_ {-1};
    bool has_transform_ {false};

    ae::animation::WeaponAnimState anim_state_ {};
    bool reload_active_ {false};
    float reload_timer_ {0.0F};
    ReloadPhase reload_phase_ {ReloadPhase::Idle};
    float reload_normalized_ {0.0F};
    float reload_ik_offset_[3] {0.0F, 0.0F, 0.0F};
    std::array<float, 16> transform_ {};

    // Melee state
    bool melee_active_ {false};
    float melee_timer_ {0.0F};
    float melee_duration_ {0.6F};
    int melee_phase_ {0}; // 0=idle, 1=windup, 2=strike, 3=recover

    std::unordered_map<int, WeaponAnimProfile> profiles_;
};

}  // namespace ahamkara::client
