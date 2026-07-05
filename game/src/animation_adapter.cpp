#include "ahamkara/game/animation_adapter.h"
#include "ae/animation/animation_driver.h"
#include "ae/animation/animation_graph.h"
#include "ae/animation/character_weapon.h"

#include <algorithm>
#include <cmath>

namespace ahamkara::game {

namespace {
constexpr float kPi = 3.14159265358979323846F;
constexpr float kHitReactionDuration = 0.25F;
}  // namespace

AnimationAdapter::AnimationAdapter()
    : char_anim_(std::make_unique<ae::animation::CharacterAnimInstance>())
    , driver_(std::make_unique<ae::animation::AnimationDriver>())
    , graph_(std::make_unique<ae::animation::AnimationGraph>()) {

    // Configure character anim instance skeleton (stub — real skeleton from glTF)
    std::vector<int> parents(64, -1);
    char_anim_->set_skeleton(64, parents);

    // Set up locomotion state machine with basic states
    char_anim_->locomotion_sm().add_state("Idle", "anim_idle");
    char_anim_->locomotion_sm().add_state("Walk", "anim_walk");
    char_anim_->locomotion_sm().add_state("Sprint", "anim_sprint");
    char_anim_->locomotion_sm().add_transition("Idle", "Walk", "start_moving", 0.15F);
    char_anim_->locomotion_sm().add_transition("Walk", "Idle", "stop_moving", 0.15F);
    char_anim_->locomotion_sm().add_transition("Walk", "Sprint", "start_sprinting", 0.10F);
    char_anim_->locomotion_sm().add_transition("Sprint", "Walk", "stop_sprinting", 0.10F);
    char_anim_->locomotion_sm().set_initial_state("Idle");

    // Upper body state machine: relaxed / aiming / firing
    char_anim_->upper_body_sm().add_state("Relaxed", "anim_upper_relaxed");
    char_anim_->upper_body_sm().add_state("Aim", "anim_upper_aim");
    char_anim_->upper_body_sm().add_state("Fire", "anim_upper_fire");
    char_anim_->upper_body_sm().add_transition("Relaxed", "Aim", "start_aiming", 0.15F);
    char_anim_->upper_body_sm().add_transition("Aim", "Relaxed", "stop_aiming", 0.15F);
    char_anim_->upper_body_sm().add_transition("Aim", "Fire", "fire", 0.05F);
    char_anim_->upper_body_sm().add_transition("Fire", "Aim", "fire_end", 0.10F);
    char_anim_->upper_body_sm().set_initial_state("Relaxed");

    // Initialize driver with the state machines
    driver_->init_locomotion(char_anim_->locomotion_sm());
    driver_->init_upper_body(char_anim_->upper_body_sm());

    // Configure default weapon animation parameters
    weapon_anim_config_.sway_amplitude = 0.0075F;
    weapon_anim_config_.sway_frequency = 1.35F;
    weapon_anim_config_.sway_damping = 4.0F;
    weapon_anim_config_.bob_amplitude_vertical = 0.010F;
    weapon_anim_config_.bob_amplitude_horizontal = 0.005F;
    weapon_anim_config_.bob_frequency_walk = 2.2F;
    weapon_anim_config_.bob_frequency_sprint = 3.75F;
    weapon_anim_config_.ads_transition_time = 0.18F;
    weapon_anim_config_.ads_sway_multiplier = 0.28F;
    driver_->set_weapon_config(weapon_anim_config_);

    joint_pose_.reserve(256);
}

AnimationAdapter::~AnimationAdapter() = default;

void AnimationAdapter::set_movement(float speed, bool is_moving, bool is_sprinting) {
    move_speed_ = speed;
    is_moving_ = is_moving;
    is_sprinting_ = is_sprinting;
}

void AnimationAdapter::set_aim(float yaw, float pitch) {
    aim_yaw_ = yaw;
    aim_pitch_ = pitch;
    char_anim_->set_aim_offset(yaw, pitch);
}

void AnimationAdapter::set_weapon(int weapon_index, bool is_firing, bool is_reloading) {
    weapon_index_ = weapon_index;
    is_firing_ = is_firing;
    is_reloading_ = is_reloading;
}

void AnimationAdapter::set_health(float health, float max_health) {
    health_ = health;
    max_health_ = max_health;
}

void AnimationAdapter::trigger_hit_reaction() {
    hit_reaction_timer_ = kHitReactionDuration;
}

bool AnimationAdapter::trigger_melee() {
    if (melee_timer_ > 0.0F) return false;
    melee_timer_ = melee_duration_;
    melee_phase_ = 1;
    return true;
}

float AnimationAdapter::melee_normalized() const {
    if (melee_duration_ <= 0.0F) return 0.0F;
    return std::clamp(1.0F - (melee_timer_ / melee_duration_), 0.0F, 1.0F);
}

void AnimationAdapter::tick(float dt) {
    // --- Drive state machine triggers from gameplay state ---
    if (is_moving_ && !is_sprinting_) {
        char_anim_->locomotion_sm().trigger("start_moving");
    } else {
        char_anim_->locomotion_sm().trigger("stop_moving");
    }
    if (is_sprinting_) {
        char_anim_->locomotion_sm().trigger("start_sprinting");
    }

    if (is_firing_) {
        char_anim_->upper_body_sm().trigger("fire");
    } else if (aim_yaw_ != 0.0F || aim_pitch_ != 0.0F) {
        char_anim_->upper_body_sm().trigger("start_aiming");
    }

    // Set locomotion blend from move speed
    char_anim_->set_locomotion_blend(move_speed_ / 7.0F);  // normalize to ~7 m/s max
    char_anim_->set_on_ground(true);

    // --- Tick character animation (produces joint pose) ---
    char_anim_->tick(dt, joint_pose_);

    // --- Build AnimGameplayInput for the driver ---
    ae::animation::AnimGameplayInput input {};
    input.speed = move_speed_;
    input.speed_normalized = std::min(move_speed_ / 7.0F, 1.0F);
    input.is_on_ground = true;
    input.aim_yaw = aim_yaw_;
    input.aim_pitch = aim_pitch_;
    input.is_firing = is_firing_;
    input.is_reloading = is_reloading_;
    input.health = health_;

    // --- Tick animation driver with gameplay input ---
    driver_->tick(input, dt,
                  char_anim_->locomotion_sm(),
                  *graph_,
                  weapon_anim_state_,
                  weapon_transform_,
                  joint_pose_);

    // --- Hit reaction (flinch) ---
    if (hit_reaction_timer_ > 0.0F) {
        hit_reaction_timer_ = std::max(0.0F, hit_reaction_timer_ - dt);
    }

    // --- Melee animation ---
    if (melee_timer_ > 0.0F) {
        melee_timer_ = std::max(0.0F, melee_timer_ - dt);
        const float t = melee_normalized();
        if (t < 0.3F) melee_phase_ = 1;
        else if (t < 0.7F) melee_phase_ = 2;
        else melee_phase_ = 3;
        if (melee_timer_ <= 0.0F) melee_phase_ = 0;
    }
}

}  // namespace ahamkara::game
