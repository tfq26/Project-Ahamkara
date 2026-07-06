#pragma once

#include "ahamkara/game/gameplay_types.h"

#include <array>
#include <cstddef>

namespace ahamkara::client {

/// Viewmodel orientation for a weapon.  Stored here (client layer) rather than
/// in the game layer so the gameplay registry stays pure of any presentation
/// concern.  See tools/blender/weapons/meshes/ for the authoritative mesh specs.
struct WeaponViewmodelTransform {
    // Rotation offsets (degrees) — applied after barrel-to-view-space correction
    float pitch_deg {0.0F};
    float yaw_deg {0.0F};
    float roll_deg {0.0F};

    // Position offset from camera anchor (meters).
    // Positive values shift right (+x), up (+y), forward/into-screen (+z).
    float pos_right   {0.0F};
    float pos_up      {0.0F};
    float pos_forward {0.0F};

    // FOV scale factor — the viewmodel is rendered at world_FOV * fov_scale.
    // Values < 1.0 make the weapon appear slightly larger / closer, which
    // improves visibility in first-person. Professional FPS titles commonly
    // use 0.80–0.90 for this.
    float fov_scale {1.0F};
};

constexpr std::size_t kWeaponViewmodelCount = 3;

// Per-weapon viewmodel transforms.  Order matches weapon_index convention:
//   0 = AR-15, 1 = Shotgun, 2 = Rocket Launcher.
//
// Fields: pitch_deg, yaw_deg, roll_deg, pos_right, pos_up, pos_forward, fov_scale
//
// Tuning notes:
//   - AR-15 sits naturally at the player's shoulder with a hint of downward tilt.
//   - Shotgun is bulkier — shifted further right/down for a heavier feel.
//   - Rocket Launcher is moved further down+left so the tube clears the screen center.
inline const std::array<WeaponViewmodelTransform, kWeaponViewmodelCount> kWeaponViewmodelTransforms = {{
    { -2.0F,   0.0F, 0.0F,  0.05F, -0.05F,  0.05F, 0.85F },  // AR-15
    { -3.0F,   0.0F, 0.0F,  0.08F, -0.08F,  0.03F, 0.80F },  // Shotgun
    { -5.0F,   0.0F, 2.0F,  0.12F, -0.15F,  0.02F, 0.90F },  // Rocket Launcher
}};

/// Resolve a weapon index to its compiled viewmodel mesh path.
/// The path format is "assets/compiled/models/viewmodel_<name>.aemesh".
inline const char* weapon_viewmodel_mesh_path(int index) {
    switch (index) {
        case 0: return "assets/compiled/models/viewmodel_ar15.aemesh";
        case 1: return "assets/compiled/models/viewmodel_shotgun.aemesh";
        case 2: return "assets/compiled/models/viewmodel_rocket_launcher.aemesh";
        default: return "assets/compiled/models/viewmodel_arms.aemesh";
    }
}

inline WeaponViewmodelTransform weapon_viewmodel_transform(int index) {
    if (index >= 0 && static_cast<std::size_t>(index) < kWeaponViewmodelTransforms.size()) {
        return kWeaponViewmodelTransforms[static_cast<std::size_t>(index)];
    }
    return {};
}

}  // namespace ahamkara::client
