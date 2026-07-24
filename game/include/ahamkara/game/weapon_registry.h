#pragma once

#include "ahamkara/game/gameplay_types.h"

#include <array>
#include <cstddef>

namespace ahamkara::game {

constexpr std::size_t kWeaponRegistrySize = 3;

// Flashback weapon lineup — gameplay definitions only.
//
// Viewmodel meshes, transforms, and animation data are owned by the client
// presentation layer (client/include/ahamkara/client/weapon_viewmodel_data.h).
// Perks and archetype authoring live in tools/blender/weapons/.
//
// Current weapons:
//   Slot 0 (Primary):   AR-15          — 400 RPM, 50-round mag, 20 dmg/bullet, automatic hitscan
//   Slot 1 (Secondary): Shotgun        — 100 RPM, 8 pellets × 10 dmg, 8-shell mag, hitscan
//   Slot 2 (Heavy):     Rocket Launcher — 30 RPM, 1-round mag, 100 dmg projectile

inline const std::array<WeaponDefinition, kWeaponRegistrySize> kWeaponRegistry = {{
    // 0: AR-15 (primary) — 400 RPM auto
    {
        .magazine_size = 50,
        .base_damage = 20.0F,
        .headshot_multiplier = 2.0F,
        .fire_mode = FireMode::Auto,
        .slot = WeaponSlot::Primary,
        .rpm = 400.0F,
        .reload_time_s = 2.0F,
        .reserve_ammo_max = 150,
        .recoil_pattern = {{0.25F, 0.08F}, {0.30F, -0.12F}, {0.28F, 0.05F}, {0.35F, -0.15F}, {0.32F, 0.10F}},
        .burst_rounds = 3,
        .burst_interval = 0.0F,
        .spread_angle = 0.5F,
        .spread_per_shot = 0.3F,
        .spread_recovery = 8.0F,
    },
    // 1: Shotgun (secondary) — 100 RPM single-shot with pellet spread
    {
        .magazine_size = 8,
        .base_damage = 10.0F,
        .headshot_multiplier = 1.5F,
        .fire_mode = FireMode::Single,
        .slot = WeaponSlot::Secondary,
        .rpm = 100.0F,
        .reload_time_s = 2.5F,
        .reserve_ammo_max = 32,
        .recoil_pattern = {{3.0F, 0.0F}},
        .burst_rounds = 1,
        .burst_interval = 0.0F,
        .spread_angle = 5.0F,
        .spread_per_shot = 0.0F,
        .spread_recovery = 0.0F,
    },
    // 2: Rocket Launcher (melee slot repurposed) — single-shot projectile
    {
        .magazine_size = 1,
        .base_damage = 100.0F,
        .headshot_multiplier = 1.0F,
        .fire_mode = FireMode::Single,
        .slot = WeaponSlot::Melee,
        .rpm = 30.0F,
        .reload_time_s = 3.0F,
        .reserve_ammo_max = 6,
        .recoil_pattern = {},
        .burst_rounds = 1,
        .burst_interval = 0.0F,
        .spread_angle = 0.0F,
        .spread_per_shot = 0.0F,
        .spread_recovery = 0.0F,
    },
}};

inline const char* weapon_name(int index) {
    switch (index) {
        case 0: return "AR-15";
        case 1: return "Shotgun";
        case 2: return "Rocket Launcher";
        default: return "Unknown";
    }
}

} // namespace ahamkara::game
