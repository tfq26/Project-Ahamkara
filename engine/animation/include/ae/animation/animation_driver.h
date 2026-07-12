#pragma once

#include "ae/animation/types.h"
#include "ae/animation/state_machine.h"
#include "ae/animation/animation_graph.h"
#include "ae/animation/character_weapon.h"
#include "ae/animation/aim_recoil.h"

#include <cstdint>

namespace ae::animation {

// ============================================================
// Animation Driver
//
// The bridge between gameplay simulation and the animation system.
//
// Purpose:
//   Takes gameplay state (movement, weapons, health, etc.) and
//   translates it into animation parameters that drive the
//   state machine, blend trees, IK, and procedural systems.
//
// This is intentionally decoupled from rendering. The driver
// produces joint matrices that the renderer consumes for skinning.
// It does NOT know about VBOs, shaders, or draw calls.
//
// Data flow:
//
//   Gameplay (World/ECS)
//       │
//       ├─→ MovementState, velocity, is_on_ground, yaw, pitch
//       ├─→ Weapon state: is_firing, is_reloading, is_ads, ammo
//       ├─→ Health, damage events
//       │
//       ▼
//   AnimationDriver
//       ├─→ StateMachine triggers (e.g., "start_moving", "jump")
//       ├─→ Blend parameters (speed, direction)
//       ├─→ AimOffset pitch/yaw → additive transforms
//       ├─→ Recoil impulses → additive transforms
//       ├─→ IK foot targets → joint corrections
//       │
//       ▼
//   AnimationGraph
//       ├─→ Evaluates clips into poses
//       ├─→ Blends multiple poses
//       ├─→ Applies additive layers
//       │
//       ▼
//   Joint Matrices (std::vector<Mat4>)
//       │
//       ▼
//   Renderer (GPU skinning shader)
// ============================================================

// ============================================================
// Gameplay input to animation — the data the driver consumes
// ============================================================

/// Movement state as seen by the animation system.
/// Maps directly from game::MovementState.
enum class AnimMovementState : std::uint8_t {
    Idle,
    Walking,
    Sprinting,
    Sliding,
    Jumping,     // includes falling
    OnLadder,
    LedgeGrab,
    Mantling,
    Crouching,
};

/// Per-frame gameplay data fed to the animation driver
struct AnimGameplayInput {
    // Movement
    AnimMovementState movement {AnimMovementState::Idle};
    float speed {0.0F};              // current horizontal speed (m/s)
    float speed_normalized {0.0F};   // 0..1 relative to max speed
    bool is_on_ground {true};
    float ground_y {0.0F};           // world Y of the ground under the character

    // Looking / aiming
    float body_yaw {0.0F};           // body facing direction (radians)
    float aim_yaw {0.0F};            // look yaw (radians)
    float aim_pitch {0.0F};          // look pitch (radians)
    float look_delta_x {0.0F};       // mouse delta this frame (for weapon sway)
    float look_delta_y {0.0F};

    // Weapon
    bool is_firing {false};
    bool fire_pressed_this_frame {false};
    bool is_reloading {false};
    bool is_ads {false};             // aim down sights
    int ammo_current {30};
    int ammo_max {30};

    // Health
    float health {100.0F};
    float health_previous {100.0F};  // for damage reaction detection
};

// ============================================================
// The Animation Driver
// ============================================================

class AnimationDriver {
public:
    AnimationDriver() = default;

    // --- Initialization ---

    /// Configure the character locomotion state machine.
    /// The caller sets up states and transitions; the driver triggers them.
    void init_locomotion(StateMachine& sm);

    /// Configure the character upper-body state machine.
    void init_upper_body(StateMachine& sm);

    /// Set the weapon animation configuration.
    void set_weapon_config(const WeaponAnimConfig& config);

    // --- Runtime tick ---

    /// Feed gameplay state and produce animation output.
    ///
    /// @param input    Current frame gameplay data.
    /// @param dt       Delta time in seconds.
    /// @param sm       Locomotion state machine (mutated in place).
    /// @param graph    Animation graph for clip evaluation.
    /// @param weapon_state  Weapon animation state (mutated in place).
    /// @param recoil_state  Recoil state (mutated in place).
    /// @param out_pose Output joint matrices for rendering.
    /// @param out_weapon_transform  Output weapon model matrix for FP rendering.
    void tick(const AnimGameplayInput& input, float dt,
              StateMachine& sm,
              AnimationGraph& graph,
              WeaponAnimState& weapon_state,
              render::Mat4& out_weapon_transform,
              std::vector<render::Mat4>& out_pose);

    /// Compute aim offset additive transforms for third-person.
    void compute_aim_offset(const AnimGameplayInput& input,
                            const AimOffsetConfig& config,
                            std::vector<JointTransform>& out_additive);

private:
    AnimMovementState previous_movement_ {AnimMovementState::Idle};
    bool previous_on_ground_ {true};
    bool previous_firing_ {false};
    WeaponAnimConfig weapon_config_;
};

// ============================================================
// Networked Animation Compression
//
// Plans for future implementation:
//
// Animation state is quantized for network replication:
//   - State machine: current state ID (enum, 4-6 bits)
//   - Blend parameters: 8-10 bit quantized floats
//   - Normalized clip time: 8-10 bit quantized
//   - Crossfade weight: 7-8 bit quantized
//   - Aim offset yaw/pitch: 10-bit each (quantized radians)
//
// Total: ~6-8 bytes per character per snapshot.
//
// Client-side: decompress and drive local animation instances.
// ============================================================

/// Compressed animation state for network replication.
/// 8 bytes total, suitable for server snapshots.
struct alignas(8) CompressedAnimState {
    // Layout: explicit bytes to guarantee 8-byte size across all compilers.
    // Byte 0: state_id (6 bits) | blend_param high 2 bits
    // Byte 1: blend_param low 8 bits
    // Byte 2: clip_time
    // Byte 3: crossfade_weight (7 bits) | is_transitioning (1 bit)
    // Byte 4-5: aim_yaw (10 bits) | aim_pitch high 6 bits
    // Byte 6: aim_pitch low 8 bits
    // Byte 7: flags (4 bits) | padding (4 bits)
    std::uint8_t state_id_: 6;         // AnimMovementState enum
    std::uint8_t blend_param_hi: 2;    // blend_param bits 8-9
    std::uint8_t blend_param_lo;
    std::uint8_t clip_time_;
    std::uint8_t crossfade_weight_: 7; // quantized 0..127 → 0..1
    std::uint8_t is_transitioning_: 1;
    std::uint16_t aim_yaw_: 10;        // quantized radians (-π..π)
    std::uint16_t aim_pitch_hi: 6;     // aim_pitch bits 8-9
    std::uint8_t aim_pitch_lo;
    std::uint8_t flags_: 4;            // is_ads, is_firing, is_on_ground, etc.
    std::uint8_t : 4;                  // padding
};
static_assert(sizeof(CompressedAnimState) == 8, "CompressedAnimState must be 8 bytes");

/// Quantize a float in [0,1] to a uint8_t
inline std::uint8_t quantize_normalized(float value) {
    if (value <= 0.0F) return 0;
    if (value >= 1.0F) return 255;
    return static_cast<std::uint8_t>(value * 255.0F);
}

/// Dequantize a uint8_t to a float in [0,1]
inline float dequantize_normalized(std::uint8_t value) {
    return static_cast<float>(value) / 255.0F;
}

}  // namespace ae::animation
