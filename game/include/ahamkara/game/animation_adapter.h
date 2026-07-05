#pragma once

#include "ae/animation/character_weapon.h"
#include "ae/render/skeletal_animation.h"

#include <memory>
#include <vector>

namespace ae::animation {
class AnimationDriver;
class AnimationGraph;
class CharacterAnimInstance;
}  // namespace ae::animation

namespace ahamkara::game {

struct Vec3;
struct PlayerInputCommand;

/// AnimationAdapter — bridges gameplay state into the animation runtime.
///
/// Owns CharacterAnimInstance (which wraps state machine, IK, aim offset),
/// AnimationDriver, AnimationGraph, and weapon animation state.
/// Call set_movement/set_aim etc. each frame from gameplay state, then tick()
/// to advance the runtime, and finally access joint_pose() for rendering.
///
/// This class lives in the game library but only links ae_animation in GUI
/// builds (guarded by if(TARGET ae_animation) in CMakeLists.txt).
class AnimationAdapter {
public:
    AnimationAdapter();
    ~AnimationAdapter();

    AnimationAdapter(const AnimationAdapter&) = delete;
    AnimationAdapter& operator=(const AnimationAdapter&) = delete;
    AnimationAdapter(AnimationAdapter&&) = delete;
    AnimationAdapter& operator=(AnimationAdapter&&) = delete;

    /// Feed gameplay state into the animation runtime. Call before tick().
    void set_movement(float speed, bool is_moving, bool is_sprinting);
    void set_aim(float yaw, float pitch);
    void set_weapon(int weapon_index, bool is_firing, bool is_reloading);
    void set_health(float health, float max_health);

    /// Trigger a hit reaction (flinch). Called when the local player takes damage.
    void trigger_hit_reaction();

    /// Trigger a melee swing. Returns false if already in a melee animation.
    bool trigger_melee();

    /// Advance the animation state machines. Call once per physics tick.
    void tick(float dt);

    /// After tick(), extract the joint pose for rendering.
    [[nodiscard]] const std::vector<ae::render::Mat4>& joint_pose() const { return joint_pose_; }
    [[nodiscard]] int joint_count() const { return static_cast<int>(joint_pose_.size()); }

    [[nodiscard]] bool is_melee_active() const { return melee_timer_ > 0.0F; }
    [[nodiscard]] float melee_normalized() const;

private:
    std::unique_ptr<ae::animation::CharacterAnimInstance> char_anim_;
    std::unique_ptr<ae::animation::AnimationDriver> driver_;
    std::unique_ptr<ae::animation::AnimationGraph> graph_;

    ae::animation::WeaponAnimState weapon_anim_state_ {};
    ae::animation::WeaponAnimConfig weapon_anim_config_ {};
    ae::render::Mat4 weapon_transform_ {};

    std::vector<ae::render::Mat4> joint_pose_;

    float move_speed_ {0.0F};
    bool is_moving_ {false};
    bool is_sprinting_ {false};
    float aim_yaw_ {0.0F};
    float aim_pitch_ {0.0F};
    int weapon_index_ {0};
    bool is_firing_ {false};
    bool is_reloading_ {false};
    float health_ {100.0F};
    float max_health_ {100.0F};

    float hit_reaction_timer_ {0.0F};
    float melee_timer_ {0.0F};
    float melee_duration_ {0.6F};
    int melee_phase_ {0};
};

}  // namespace ahamkara::game
