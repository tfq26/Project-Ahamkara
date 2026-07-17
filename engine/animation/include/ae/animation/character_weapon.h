#pragma once

#include "ae/animation/types.h"
#include "ae/animation/state_machine.h"
#include "ae/animation/ik.h"
#include "ae/animation/aim_recoil.h"

#include <unordered_map>
#include <vector>

namespace ae::animation {

// ============================================================
// Character Animation State — Third Person
//
// Manages the full-body animation for a character entity rendered
// in third-person view (both player's own character in 3P mode and
// other players/NPCs).
//
// Owns:
//   - A StateMachine for locomotion (idle/walk/sprint/jump/slide)
//   - Upper-body overlay layer for weapon handling
//   - IK targets for foot/ground placement
//   - Aim offset for spine/neck
// ============================================================

class CharacterAnimInstance {
public:
    CharacterAnimInstance() = default;

    // --- Construction ---

    /// Configure the skeleton layout for this character.
    /// @param joint_count Number of joints in the skeleton.
    /// @param parent_indices Array of parent indices per joint (-1 for root).
    void set_skeleton(int joint_count, const std::vector<int>& parent_indices);

    /// Get the locomotion state machine for configuration.
    [[nodiscard]] StateMachine& locomotion_sm() { return locomotion_sm_; }
    [[nodiscard]] const StateMachine& locomotion_sm() const { return locomotion_sm_; }

    /// Get the upper-body overlay state machine (for weapon handling, etc.).
    [[nodiscard]] StateMachine& upper_body_sm() { return upper_body_sm_; }
    [[nodiscard]] const StateMachine& upper_body_sm() const { return upper_body_sm_; }

    /// Configure IK chains for foot placement.
    /// @param left_leg_chain Chain for left leg (hip → knee → ankle).
    /// @param right_leg_chain Chain for right leg.
    void set_leg_ik(const IKChain& left_leg_chain, const IKChain& right_leg_chain);

    /// Configure aim offset for third-person spine/neck bending.
    void set_aim_offset_config(const AimOffsetConfig& config);

    // --- Runtime ---

    /// Tick all animation systems. Produces a final pose in `out_matrices`.
    /// @param dt Delta time in seconds.
    /// @param out_matrices Output: one Mat4 per joint for skinning.
    void tick(float dt, std::vector<skeleton::Mat4>& out_matrices);

    /// Set the aim direction offset (relative to body facing).
    void set_aim_offset(float yaw, float pitch);

    /// Set foot placement targets (world-space positions for ankle joints).
    void set_foot_targets(const IKTarget& left_foot, const IKTarget& right_foot,
                          float left_weight, float right_weight);

    /// Set the locomotion blend parameter (typically speed/movement magnitude).
    void set_locomotion_blend(float speed_param);

    /// Set whether the character is on the ground (affects jump/fall blending).
    void set_on_ground(bool on_ground);

    // --- Query ---

    [[nodiscard]] const std::vector<skeleton::Mat4>& last_pose() const { return last_pose_; }
    [[nodiscard]] int joint_count() const { return joint_count_; }

private:
    StateMachine locomotion_sm_;
    StateMachine upper_body_sm_;
    IKSolver ik_solver_;
    AimOffsetConfig aim_config_;
    AimOffsetState aim_state_;

    std::vector<int> parent_indices_;
    std::vector<skeleton::Mat4> last_pose_;
    int joint_count_ {0};

    int left_leg_ik_chain_ {-1};
    int right_leg_ik_chain_ {-1};
    float left_foot_weight_ {0.0F};
    float right_foot_weight_ {0.0F};
};

// ============================================================
// Weapon Animation State — First Person
//
// Manages first-person weapon viewmodel animations:
//   - Idle sway (subtle weapon movement)
//   - Fire animation
//   - Reload animation
//   - ADS (aim down sights) transitions
//   - Procedural recoil
//   - Movement bob (bobbing while walking/sprinting)
//
// The weapon is treated as a separate skeleton (typically 3-5 bones:
// root, magazine, bolt/slide, trigger, etc.) or driven via
// procedural transforms on a single weapon mesh.
// ============================================================

struct WeaponAnimConfig {
    // Sway
    float sway_amplitude {0.008F};       // meters
    float sway_frequency {1.2F};         // Hz
    float sway_damping {3.0F};           // how fast sway follows movement

    // Movement bob
    float bob_amplitude_vertical {0.012F};
    float bob_amplitude_horizontal {0.006F};
    float bob_frequency_walk {2.0F};     // Hz at walk speed
    float bob_frequency_sprint {3.5F};   // Hz at sprint speed

    // ADS
    float ads_transition_time {0.2F};    // seconds to enter/exit ADS
    float ads_sway_multiplier {0.3F};    // sway reduction when ADS

    // Recoil
    RecoilConfig recoil;

    // Weapon bone index (which joint the weapon mesh follows)
    int weapon_root_joint {0};
};

/// Runtime weapon animation state
struct WeaponAnimState {
    float sway_phase {0.0F};
    float sway_velocity_x {0.0F};
    float sway_velocity_y {0.0F};
    float bob_phase {0.0F};
    float ads_blend {0.0F};              // 0 = hipfire, 1 = fully ADS
    float fire_anim_time {0.0F};         // timer for fire animation
    RecoilState recoil;
    bool is_firing {false};
    bool is_reloading {false};
    float reload_anim_time {0.0F};
};

// ============================================================
// Layer evaluation functions
//
// Each function produces a weapon-space transform offset for one
// animation layer. Callers compose layers by multiplying:
//   final = sway_offset * bob_offset * recoil_offset
//
// Layers are intentionally independent:
//   - Sway: mouse-driven subtle lateral motion
//   - Bob: movement-driven vertical/rhythmic oscillation
//   - Recoil kick: brief impulse kick on fire events
//
// New weapon types reuse these layers by providing a tuned
// WeaponAnimConfig. The layering system is generic and does not
// depend on weapon-specific logic.
// ============================================================

/// Evaluate weapon sway layer (mouse look → lateral motion).
/// @param state Must have valid sway_phase, sway_velocity_x/y.
/// @param config Sway parameters (amplitude, frequency, damping).
/// @param dt Delta time in seconds.
/// @param look_delta_x Horizontal mouse delta this frame.
/// @param look_delta_y Vertical mouse delta this frame.
/// @param ads_blend Current ADS blend factor [0,1] for sway reduction.
/// @param out_offset Output: sway-only transform offset.
void evaluate_sway_layer(WeaponAnimState& state, const WeaponAnimConfig& config,
                          float dt, float look_delta_x, float look_delta_y,
                          float ads_blend, skeleton::Mat4& out_offset);

/// Evaluate weapon bob layer (movement → vertical/horizontal oscillation).
/// @param state Must have valid bob_phase.
/// @param config Bob parameters (amplitudes, frequencies).
/// @param dt Delta time in seconds.
/// @param player_speed Current player speed (m/s).
/// @param is_moving Whether the player is moving.
/// @param out_offset Output: bob-only transform offset.
void evaluate_bob_layer(WeaponAnimState& state, const WeaponAnimConfig& config,
                         float dt, float player_speed, bool is_moving,
                         skeleton::Mat4& out_offset);

/// Evaluate weapon recoil kick layer (fire impulse → brief kick + recovery).
/// @param state Must have valid fire_anim_time; call fire_weapon_kick() to trigger a kick.
/// @param config Recoil kick parameters (implicit in fire_anim_time max).
/// @param dt Delta time in seconds.
/// @param out_offset Output: recoil-kick-only transform offset.
void evaluate_recoil_kick_layer(WeaponAnimState& state, const WeaponAnimConfig& config,
                                 float dt, skeleton::Mat4& out_offset);

/// Trigger a recoil kick impulse on the next evaluate_recoil_kick_layer() call.
/// Sets the fire_anim_time so the recoil layer produces a brief upward kick.
void fire_weapon_kick(WeaponAnimState& state);

/// Evaluate first-person weapon animation and produce a weapon-space transform.
/// Convenience wrapper that composes all three layers internally.
/// @param state Runtime state (updated in place).
/// @param config Weapon configuration.
/// @param dt Delta time.
/// @param player_speed Current player speed (m/s, for bob/sway).
/// @param look_delta Mouse/controller look delta this frame.
/// @param is_moving Whether the player is moving.
/// @param out_transform Output weapon root transform (applied as model matrix).
void evaluate_weapon_animation(WeaponAnimState& state, const WeaponAnimConfig& config,
                                float dt, float player_speed,
                                float look_delta_x, float look_delta_y,
                                bool is_moving, bool is_ads, bool fire_pressed,
                                skeleton::Mat4& out_transform);

}  // namespace ae::animation
