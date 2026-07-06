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
|};

/// Per-weapon grip socket positions that define where the character's hands
/// attach to the weapon.  Positions are in viewmodel-local space (the same
/// coordinate system as the viewmodel_arms skeleton).
///
/// These are used by the arm IK solver to position the hand end effectors
/// at the correct weapon grip points, keeping hands locked to the weapon
/// during idle sway, movement bob, and recoil.
struct WeaponGripSockets {
    // Right hand grip position (the shooting hand on trigger/grip).
    float grip_right_x {0.0F};
    float grip_right_y {0.0F};
    float grip_right_z {0.0F};

    // Left hand foregrip position (support hand on foreguard/magwell).
    // Used for two-handed IK when a left arm is present.
    float grip_left_x {0.0F};
    float grip_left_y {0.0F};
    float grip_left_z {0.0F};
|};

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

/// Per-weapon grip socket positions.  The right hand grip targets the
/// weapon's pistol grip / trigger area.  Values are in viewmodel-local
/// (model) space and correspond to the weapon_attach bone position
/// (~0, 0.75, 0 in bind pose) with per-weapon offsets.
///
/// These offsets tune how the hand sits on each weapon.  A weapon with a
/// longer foreguard pulls the left hand forward; a different grip angle
/// shifts the right hand.
inline const std::array<WeaponGripSockets, kWeaponViewmodelCount> kWeaponGripSockets = {{
    // AR-15 — right hand on grip, left hand on foreguard
    { 0.00F, 0.70F, 0.00F,   // right hand grip
      0.15F, 0.55F, 0.00F }, // left hand foregrip

    // Shotgun — right hand on grip, left hand further forward on pump
    { 0.00F, 0.70F, 0.00F,   // right hand grip
      0.20F, 0.50F, 0.00F }, // left hand foregrip

    // Rocket Launcher — right hand on grip, left hand on front tube
    { 0.00F, 0.70F, 0.00F,   // right hand grip
      0.25F, 0.40F, 0.00F }, // left hand foregrip
}};

/// Resolve grip sockets for a weapon index.
inline WeaponGripSockets weapon_grip_sockets(int index) {
    if (index >= 0 && static_cast<std::size_t>(index) < kWeaponGripSockets.size()) {
        return kWeaponGripSockets[static_cast<std::size_t>(index)];
    }
    return {};
}

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
