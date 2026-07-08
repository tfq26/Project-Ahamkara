#include "ahamkara/game/item_registry.h"

namespace ahamkara::game {

ItemRegistry& ItemRegistry::instance() {
    static ItemRegistry inst;
    return inst;
}

void ItemRegistry::register_item(const ItemDefinition& def) {
    items_[def.definition_id] = def;
}

void ItemRegistry::register_perk(const PerkDefinition& def) {
    perks_[def.perk_id] = def;
}

const ItemDefinition* ItemRegistry::get_item(std::uint32_t id) const {
    auto it = items_.find(id);
    if (it != items_.end()) {
        return &it->second;
    }
    return nullptr;
}

const PerkDefinition* ItemRegistry::get_perk(std::uint32_t id) const {
    auto it = perks_.find(id);
    if (it != perks_.end()) {
        return &it->second;
    }
    return nullptr;
}

void ItemRegistry::clear() {
    items_.clear();
    perks_.clear();
}

void ItemRegistry::register_default_items() {
    clear();

    // --- 1. Register Default Perks ---
    
    // Outlaw (Lua-scripted)
    PerkDefinition outlaw;
    outlaw.perk_id = 1001;
    outlaw.display_name = "Outlaw";
    outlaw.description = "Precision kills greatly decrease reload time.";
    outlaw.type = PerkType::LuaScriptedBehavior;
    outlaw.lua_script_path = "assets/scripts/perks/outlaw.lua";
    register_perk(outlaw);

    // Kill Clip (Lua-scripted)
    PerkDefinition kill_clip;
    kill_clip.perk_id = 1002;
    kill_clip.display_name = "Kill Clip";
    kill_clip.description = "Reloading after a kill grants increased damage.";
    kill_clip.type = PerkType::LuaScriptedBehavior;
    kill_clip.lua_script_path = "assets/scripts/perks/kill_clip.lua";
    register_perk(kill_clip);

    // Steady Hand (Passive Modifiers)
    PerkDefinition steady_hand;
    steady_hand.perk_id = 1003;
    steady_hand.display_name = "Steady Hand";
    steady_hand.description = "Increases weapon stability and handling.";
    steady_hand.type = PerkType::PassiveStatModifier;
    steady_hand.modifiers = {
        {StatType::Stability, 10.0F, 0.0F},
        {StatType::Handling, 5.0F, 0.0F}
    };
    register_perk(steady_hand);

    // --- 2. Register Default Weapons ---
    
    // Pistol (Secondary)
    ItemDefinition pistol;
    pistol.definition_id = 2001;
    pistol.display_name = "Pistol";
    pistol.type = ItemType::Weapon;
    pistol.slot = ItemSlot::WeaponSecondary;
    pistol.base_stats = {
        {StatType::Impact, 20.0F},
        {StatType::Range, 40.0F},
        {StatType::Stability, 60.0F},
        {StatType::Handling, 70.0F},
        {StatType::ReloadSpeed, 50.0F},
        {StatType::MagazineSize, 12.0F},
        {StatType::RPM, 300.0F}
    };
    pistol.random_perk_pool = {1001, 1002, 1003};
    register_item(pistol);

    // Assault Rifle (Primary)
    ItemDefinition ar;
    ar.definition_id = 2002;
    ar.display_name = "Assault Rifle";
    ar.type = ItemType::Weapon;
    ar.slot = ItemSlot::WeaponPrimary;
    ar.base_stats = {
        {StatType::Impact, 18.0F},
        {StatType::Range, 55.0F},
        {StatType::Stability, 50.0F},
        {StatType::Handling, 50.0F},
        {StatType::ReloadSpeed, 55.0F},
        {StatType::MagazineSize, 30.0F},
        {StatType::RPM, 600.0F}
    };
    ar.random_perk_pool = {1001, 1002, 1003};
    register_item(ar);

    // Shotgun (Primary)
    ItemDefinition shotgun;
    shotgun.definition_id = 2003;
    shotgun.display_name = "Shotgun";
    shotgun.type = ItemType::Weapon;
    shotgun.slot = ItemSlot::WeaponPrimary;
    shotgun.base_stats = {
        {StatType::Impact, 80.0F},
        {StatType::Range, 15.0F},
        {StatType::Stability, 20.0F},
        {StatType::Handling, 30.0F},
        {StatType::ReloadSpeed, 40.0F},
        {StatType::MagazineSize, 6.0F},
        {StatType::RPM, 80.0F}
    };
    shotgun.random_perk_pool = {1003};
    register_item(shotgun);

    // --- 3. Register Default Armor ---

    // Helmet (Armor)
    ItemDefinition helmet;
    helmet.definition_id = 3001;
    helmet.display_name = "Iron Helmet";
    helmet.type = ItemType::Armor;
    helmet.slot = ItemSlot::ArmorHelmet;
    helmet.base_stats = {
        {StatType::Resilience, 10.0F},
        {StatType::Recovery, 8.0F},
        {StatType::Mobility, 5.0F}
    };
    register_item(helmet);

    // Gauntlets (Armor)
    ItemDefinition gauntlets;
    gauntlets.definition_id = 3002;
    gauntlets.display_name = "Iron Gauntlets";
    gauntlets.type = ItemType::Armor;
    gauntlets.slot = ItemSlot::ArmorGauntlets;
    gauntlets.base_stats = {
        {StatType::Resilience, 8.0F},
        {StatType::Recovery, 10.0F},
        {StatType::Mobility, 5.0F}
    };
    register_item(gauntlets);

    // Chest Piece (Armor)
    ItemDefinition chest;
    chest.definition_id = 3003;
    chest.display_name = "Iron Chest Piece";
    chest.type = ItemType::Armor;
    chest.slot = ItemSlot::ArmorChest;
    chest.base_stats = {
        {StatType::Resilience, 15.0F},
        {StatType::Recovery, 5.0F},
        {StatType::Mobility, 2.0F}
    };
    register_item(chest);
}

} // namespace ahamkara::game
