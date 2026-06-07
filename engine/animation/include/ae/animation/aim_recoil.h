#pragma once

#include "ae/animation/types.h"

namespace ae::animation {

// ============================================================
// Aim Offset
//
// Adds a directional offset to the spine/arm joints based on
// the player's aim direction (yaw/pitch). This is applied as
// an additive blend on top of the base animation pose.
//
// Typical use:
//   - Third-person: spine bends to match look direction
//   - First-person: arms + weapon follow aim
// ============================================================

struct AimOffsetConfig {
    // Which joints to affect
    int spine_joint {-1};
    int neck_joint {-1};
    int left_shoulder_joint {-1};
    int right_shoulder_joint {-1};

    // Maximum offset angles (radians)
    float max_spine_yaw {0.5F};       // ~29 degrees
    float max_spine_pitch {0.3F};     // ~17 degrees
    float max_neck_yaw {0.3F};
    float max_neck_pitch {0.2F};
    float max_arm_yaw {0.4F};
    float max_arm_pitch {0.6F};

    // How much of the total look angle each bone takes (%)
    float spine_yaw_share {0.4F};     // 40% of yaw goes to spine
    float neck_yaw_share {0.3F};      // 30% to neck
    float spine_pitch_share {0.5F};   // 50% of pitch to spine
    float neck_pitch_share {0.3F};    // 30% to neck
};

/// Runtime aim offset state
struct AimOffsetState {
    float yaw {0.0F};       // horizontal aim angle (radians, relative to body)
    float pitch {0.0F};     // vertical aim angle (radians)
    bool active {false};
};

// ============================================================
// Procedural Recoil Animation
//
// Generates additive joint transforms that simulate weapon recoil.
// Applied on top of the base firing animation for responsive feel.
//
// The recoil is modeled as an impulse-then-recover pattern:
//   1. Fire event → apply sharp kick (up + slight random horizontal)
//   2. Over time → recover toward original position
// ============================================================

struct RecoilConfig {
    // Recoil kick per shot
    float kick_pitch_min {0.03F};    // minimum upward rotation (radians)
    float kick_pitch_max {0.06F};    // maximum upward rotation
    float kick_yaw_min {-0.02F};     // horizontal spread
    float kick_yaw_max {0.02F};
    float kick_roll_min {-0.01F};    // slight twist
    float kick_roll_max {0.01F};

    // Recoil pattern (for sustained fire)
    float pattern_scale {1.0F};      // overall intensity multiplier
    float vertical_bias {0.7F};      // 0..1, higher = more vertical
    float randomness {0.15F};         // random jitter per shot

    // Recovery
    float recovery_speed {5.0F};     // how fast to return to rest (higher = faster)
    float recovery_damping {0.8F};   // damping factor (0..1, lower = faster settle)

    // ADS (aim down sights) modifiers
    float ads_kick_multiplier {0.5F}; // recoil reduction when ADS

    // Which joint receives the recoil
    int recoil_joint {-1};           // e.g., shoulder or weapon root
};

/// Runtime recoil state
struct RecoilState {
    float current_pitch {0.0F};      // accumulated pitch offset
    float current_yaw {0.0F};
    float current_roll {0.0F};
    float recovery_velocity_pitch {0.0F};
    float recovery_velocity_yaw {0.0F};
    float recovery_velocity_roll {0.0F};
};

/// Evaluate recoil and return the joint transform offset for this frame.
/// Call this once per tick.
void apply_recoil(RecoilState& state, const RecoilConfig& config,
                  float dt, bool is_firing, bool is_ads,
                  JointTransform& out_offset);

/// Fire a single shot — adds a kick impulse.
void fire_recoil(RecoilState& state, const RecoilConfig& config, bool is_ads);

}  // namespace ae::animation
