#pragma once

#include "wish/types.h"

#include <string>
#include <string_view>
#include <vector>

namespace wish::core {

// ---------------------------------------------------------------------------
// ModifierType — generic modifier categories that can drive content toggles.
// Extend this enum when adding new data-driven modifier types.
// ---------------------------------------------------------------------------
enum class ModifierType : u8 {
    None = 0,
    DamageMultiplier = 1,
    DamageReduction = 2,
    SpeedModifier = 3,
    JumpModifier = 4,
    HealthRegen = 5,
    ShieldRegen = 6,
    SpawnRateModifier = 7,
    MaxPlayersModifier = 8,
    GravityModifier = 9,
    ScoreMultiplier = 10,
    RespawnTimeModifier = 11,
    ReloadSpeedModifier = 12,
    WeaponDamageOverride = 13,
    FriendlyFireToggle = 14,
    Custom = 255
};

/// Human-readable name for a ModifierType.
constexpr std::string_view modifier_type_name(ModifierType type) {
    switch (type) {
        case ModifierType::None:                  return "none";
        case ModifierType::DamageMultiplier:       return "damage_multiplier";
        case ModifierType::DamageReduction:         return "damage_reduction";
        case ModifierType::SpeedModifier:           return "speed_modifier";
        case ModifierType::JumpModifier:            return "jump_modifier";
        case ModifierType::HealthRegen:             return "health_regen";
        case ModifierType::ShieldRegen:             return "shield_regen";
        case ModifierType::SpawnRateModifier:       return "spawn_rate_modifier";
        case ModifierType::MaxPlayersModifier:      return "max_players_modifier";
        case ModifierType::GravityModifier:         return "gravity_modifier";
        case ModifierType::ScoreMultiplier:         return "score_multiplier";
        case ModifierType::RespawnTimeModifier:     return "respawn_time_modifier";
        case ModifierType::ReloadSpeedModifier:     return "reload_speed_modifier";
        case ModifierType::WeaponDamageOverride:    return "weapon_damage_override";
        case ModifierType::FriendlyFireToggle:      return "friendly_fire_toggle";
        case ModifierType::Custom:                  return "custom";
    }
    return "unknown";
}

/// Parse a string name back into a ModifierType.
inline ModifierType parse_modifier_type(std::string_view name) {
    if (name == "damage_multiplier")       return ModifierType::DamageMultiplier;
    if (name == "damage_reduction")         return ModifierType::DamageReduction;
    if (name == "speed_modifier")           return ModifierType::SpeedModifier;
    if (name == "jump_modifier")            return ModifierType::JumpModifier;
    if (name == "health_regen")             return ModifierType::HealthRegen;
    if (name == "shield_regen")             return ModifierType::ShieldRegen;
    if (name == "spawn_rate_modifier")      return ModifierType::SpawnRateModifier;
    if (name == "max_players_modifier")     return ModifierType::MaxPlayersModifier;
    if (name == "gravity_modifier")         return ModifierType::GravityModifier;
    if (name == "score_multiplier")         return ModifierType::ScoreMultiplier;
    if (name == "respawn_time_modifier")    return ModifierType::RespawnTimeModifier;
    if (name == "reload_speed_modifier")    return ModifierType::ReloadSpeedModifier;
    if (name == "weapon_damage_override")   return ModifierType::WeaponDamageOverride;
    if (name == "friendly_fire_toggle")     return ModifierType::FriendlyFireToggle;
    if (name == "custom")                   return ModifierType::Custom;
    return ModifierType::None;
}

// ---------------------------------------------------------------------------
// ModifierParam — a single key-value parameter for a modifier configuration.
// ---------------------------------------------------------------------------
struct ModifierParam {
    std::string_view key {};
    std::string_view value {};

    bool operator==(const ModifierParam& other) const {
        return key == other.key && value == other.value;
    }

    bool operator!=(const ModifierParam& other) const {
        return !(*this == other);
    }
};

// ---------------------------------------------------------------------------
// ModifierConfig — full modifier configuration for an activity.
// ---------------------------------------------------------------------------
struct ModifierConfig {
    ModifierType type {ModifierType::None};
    std::string_view name {};
    std::vector<ModifierParam> params {};
    bool active {true};
    float duration {0.0F};       ///< 0 = permanent / indefinite
    float remaining_time {0.0F}; ///< Updated at runtime by LiveContentModifier
    u32 rotation_order {0};      ///< Order in rotation schedule (0 = no rotation)

    bool operator==(const ModifierConfig& other) const {
        return type == other.type
            && name == other.name
            && params.size() == other.params.size()
            && active == other.active
            && duration == other.duration;
    }

    bool operator!=(const ModifierConfig& other) const {
        return !(*this == other);
    }
};

// ---------------------------------------------------------------------------
// Convenience helpers
// ---------------------------------------------------------------------------

/// Look up a parameter value by key in a modifier config.
/// Returns an empty string_view if the key is not found.
inline std::string_view find_modifier_param(const ModifierConfig& config, std::string_view key) {
    for (const auto& p : config.params) {
        if (p.key == key) return p.value;
    }
    return {};
}

/// Check whether a modifier config has an active flag and (if duration > 0)
/// still has remaining time.
inline bool is_modifier_active(const ModifierConfig& config) {
    if (!config.active) return false;
    if (config.duration > 0.0F && config.remaining_time <= 0.0F) return false;
    return true;
}

} // namespace wish::core
