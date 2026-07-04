#pragma once

#include "ahamkara/game/gameplay_types.h"

#include <array>
#include <cstddef>

namespace ahamkara::client {

/// Viewmodel orientation for a weapon.  Stored here (client layer) rather than
/// in the game layer so the gameplay registry stays pure of any presentation
/// concern.  See tools/blender/weapons/meshes/ for the authoritative mesh specs.
struct WeaponViewmodelTransform {
    float pitch_deg {0.0F};
    float yaw_deg {0.0F};
    float roll_deg {0.0F};
};

constexpr std::size_t kWeaponViewmodelCount = 3;

/// Per-weapon viewmodel transforms.  These are subtle orientation offsets that
/// make each weapon look correct in first-person.  Zeroed means the weapon's
/// authored +X barrel is shown as-is (after the renderer's shared -90° Y
/// barrel-to-view-space correction).
inline const std::array<WeaponViewmodelTransform, kWeaponViewmodelCount> kWeaponViewmodelTransforms = {{
    {0.0F, 0.0F, 0.0F},  // AR-15
    {0.0F, 0.0F, 0.0F},  // Shotgun
    {0.0F, 0.0F, 0.0F},  // Rocket Launcher
}};

/// Resolve a weapon index to its compiled viewmodel mesh path.
/// The path format is "assets/compiled/models/viewmodel_<name>.aemesh".
inline const char* weapon_viewmodel_mesh_path(int index) {
    switch (index) {
        case 0: return "assets/compiled/models/viewmodel_arms.aemesh";
        case 1: return "assets/compiled/models/viewmodel_arms.aemesh";
        case 2: return "assets/compiled/models/viewmodel_arms.aemesh";
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
