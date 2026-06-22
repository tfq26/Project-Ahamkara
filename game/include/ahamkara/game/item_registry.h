#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>

namespace ahamkara::game {

enum class StatType : std::uint8_t {
    // Weapon Stats
    Impact = 0,
    Range,
    Stability,
    Handling,
    ReloadSpeed,
    AimAssist,
    Zoom,
    AirborneEffectiveness,
    RPM,
    MagazineSize,
    
    // Armor Stats
    Mobility,
    Resilience,
    Recovery,
    Discipline,
    Intellect,
    Strength,

    Count
};

enum class ItemType : std::uint8_t {
    Weapon = 0,
    Armor,
    Subclass,
    Perk,
    Mod
};

enum class ItemSlot : std::uint8_t {
    // Weapon slots
    WeaponPrimary = 0,
    WeaponSecondary,
    WeaponMelee,
    
    // Armor slots
    ArmorHelmet,
    ArmorGauntlets,
    ArmorChest,
    ArmorLegs,
    ArmorClassItem,

    None
};

struct StatModifier {
    StatType stat {StatType::Impact};
    float flat_add {0.0F};
    float percent_add {0.0F}; // e.g. 0.10F for +10%
};

enum class PerkType : std::uint8_t {
    PassiveStatModifier = 0,
    LuaScriptedBehavior
};

struct PerkDefinition {
    std::uint32_t perk_id {0};
    std::string display_name;
    std::string description;
    PerkType type {PerkType::PassiveStatModifier};
    
    // For passive modifiers
    std::vector<StatModifier> modifiers;
    
    // For Lua-scripted perks
    std::string lua_script_path;
};

struct ItemDefinition {
    std::uint32_t definition_id {0};
    std::string display_name;
    ItemType type {ItemType::Weapon};
    ItemSlot slot {ItemSlot::None};
    
    // Base stats before perks/mods are applied
    std::vector<std::pair<StatType, float>> base_stats;
    
    // Sockets / Perk slots
    std::vector<std::uint32_t> intrinsic_perks;
    std::vector<std::uint32_t> random_perk_pool;
    
    // Asset references
    std::string icon_path;
    std::string model_path;
};

class ItemRegistry {
public:
    static ItemRegistry& instance();
    
    void register_item(const ItemDefinition& def);
    void register_perk(const PerkDefinition& def);
    
    [[nodiscard]] const ItemDefinition* get_item(std::uint32_t id) const;
    [[nodiscard]] const PerkDefinition* get_perk(std::uint32_t id) const;
    
    void clear();

    /// Register defaults so the game runs immediately with basic items/perks
    void register_default_items();

private:
    ItemRegistry() = default;
    ~ItemRegistry() = default;
    ItemRegistry(const ItemRegistry&) = delete;
    ItemRegistry& operator=(const ItemRegistry&) = delete;

    std::unordered_map<std::uint32_t, ItemDefinition> items_;
    std::unordered_map<std::uint32_t, PerkDefinition> perks_;
};

} // namespace ahamkara::game
