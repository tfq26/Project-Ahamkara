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

// ============================================================
// Reload animation phases
// ============================================================

/// Phases of a procedural reload animation sequence.
/// Each phase drives hand IK target offset and weapon tilt.
enum class ReloadPhase : int {
    Idle = 0,
    GrabMag = 1,      // hand moves from grip to magazine position
    RemoveMag = 2,     // magazine removed, hand holds magazine
    InsertMag = 3,     // new magazine inserted
    ReturnToGrip = 4,  // hand returns to grip position
};

/// Per-weapon reload animation parameters.
///
/// Phase timing is defined as fractions of the total reload duration
/// (from WeaponAnimProfile::reload_duration).  Each phase maps to a
/// [start, end] range in [0, 1].
///
/// The magazine position (mag_pos_*) is where the hand IK target moves
/// during the grab/remove/insert phases — the hand leaves the grip socket
/// and goes to the magazine well to visually manipulate the magazine.
///
/// Weapon tilt angles define the viewmodel rotation during the magazine
/// phases so the weapon pivots to expose the magwell.
struct WeaponReloadData {
    // Phase timing fractions [0, 1]
    float grab_start   {0.00F};
    float grab_end     {0.20F};
    float remove_start {0.20F};
    float remove_end   {0.50F};
    float insert_start {0.50F};
    float insert_end   {0.80F};
    float return_start {0.80F};
    float return_end   {1.00F};

    // Magazine position in viewmodel-local space
    // (relative to the weapon_attach bone / grip area)
    float mag_pos_x {0.0F};
    float mag_pos_y {0.0F};
    float mag_pos_z {0.0F};

    // Max weapon tilt during magazine phases (degrees)
    float tilt_pitch_deg {0.0F};
    float tilt_yaw_deg   {0.0F};
    float tilt_roll_deg  {0.0F};

    // Weapon position offset during magazine phases (meters)
    float offset_right   {0.0F};
    float offset_up      {0.0F};
    float offset_forward {0.0F};
};

/// Per-weapon reload animation data.  Order matches weapon_index convention:
///   0 = AR-15, 1 = Shotgun, 2 = Rocket Launcher.
///
/// Phase timing varies by weapon class — shotguns have a longer remove/insert
/// sequence, rocket launchers reload a single tube.
inline const std::array<WeaponReloadData, kWeaponViewmodelCount> kWeaponReloadData = {{
    // AR-15 — standard box magazine, quick reload
    {
        0.00F, 0.18F,   // grab
        0.18F, 0.45F,   // remove
        0.45F, 0.75F,   // insert
        0.75F, 1.00F,   // return
        0.00F, 0.30F, 0.10F,   // mag position (below grip, slightly forward)
        -18.0F, 8.0F, -5.0F,   // tilt
        -0.02F, -0.06F, -0.08F, // position offset
    },

    // Shotgun — tube-fed, longer reload (pump action)
    {
        0.00F, 0.25F,   // grab
        0.25F, 0.55F,   // remove/port open
        0.55F, 0.82F,   // insert shell
        0.82F, 1.00F,   // return
        0.00F, 0.35F, 0.15F,   // mag position (further forward, lower)
        -25.0F, 12.0F, -8.0F,  // tilt (more dramatic)
        -0.03F, -0.08F, -0.10F,
    },

    // Rocket Launcher — single tube, hand moves to rear
    {
        0.00F, 0.15F,   // grab
        0.15F, 0.45F,   // remove rear cap
        0.45F, 0.75F,   // insert new rocket
        0.75F, 1.00F,   // return
        0.00F, 0.25F, 0.25F,   // mag position (rear of tube)
        -10.0F, 5.0F, -3.0F,   // tilt (subtle — large weapon)
        -0.01F, -0.04F, -0.06F,
    },
}};

/// Resolve reload animation data for a weapon index.
inline WeaponReloadData weapon_reload_data(int index) {
    if (index >= 0 && static_cast<std::size_t>(index) < kWeaponReloadData.size()) {
        return kWeaponReloadData[static_cast<std::size_t>(index)];
    }
    return {};
}

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

// ============================================================
// ADS (Aim Down Sights) transform data
// ============================================================

/// Per-weapon ADS transform offset applied on top of the hip-fire position.
/// When fully aimed, the total viewmodel offset becomes:
///   hip_offset + ads_offset
/// The ADS offset typically cancels the hip offset to bring the weapon toward
/// screen center (e.g., ads_pos_right = -hip_pos_right).
struct WeaponAdsTransform {
    // Position offset added to hip-fire position during ADS (meters).
    float ads_pos_right   {0.0F};
    float ads_pos_up      {0.0F};
    float ads_pos_forward {0.0F};

    // Rotation offset added to hip-fire rotation during ADS (degrees).
    float ads_pitch_deg   {0.0F};
    float ads_yaw_deg     {0.0F};
    float ads_roll_deg    {0.0F};

    // World camera FOV scale during ADS (0.0-1.0).
    // The camera FOV becomes: base_FOV * ads_fov_scale.
    // E.g., 0.67 at 60° base = ~40° FOV (common FPS zoom).
    float ads_fov_scale   {1.0F};
};

// Per-weapon ADS transforms.  Order matches weapon_viewmodel_data convention:
//   0 = AR-15, 1 = Shotgun, 2 = Rocket Launcher.
//
// The position offsets negate each weapon's hip offset to bring the weapon
// toward center.  The pitch adds a slight downward tilt to align with the
// invisible sight picture.
inline const std::array<WeaponAdsTransform, kWeaponViewmodelCount> kWeaponAdsTransforms = {{
    // AR-15: cancel hip offset, slight forward+up, tight zoom
    { -0.05F,  0.05F, -0.05F,   // negate hip pos
      -2.0F,   0.0F,   0.0F,   // slight tilt down
      0.70F },                   // 42° zoom

    // Shotgun: cancel hip offset, bring fully centered
    { -0.08F,  0.08F, -0.03F,   // negate hip pos
      -2.5F,   0.0F,   0.0F,   // slight tilt down
      0.67F },                   // 40° zoom

    // Rocket Launcher: cancel hip offset, slight upward for tube clearance
    { -0.12F,  0.15F, -0.02F,   // negate hip pos
      -3.0F,   0.0F,   0.0F,   // tilt down more for sight picture
      0.75F },                   // 45° zoom
}};

inline WeaponAdsTransform weapon_ads_transform(int index) {
    if (index >= 0 && static_cast<std::size_t>(index) < kWeaponAdsTransforms.size()) {
        return kWeaponAdsTransforms[static_cast<std::size_t>(index)];
    }
    return {};
}

}  // namespace ahamkara::client
