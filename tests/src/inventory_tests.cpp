#include "ahamkara/game/inventory.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

using namespace ahamkara::game;

// ===================================================================
//  ItemInstance
// ===================================================================

void test_item_instance_defaults() {
    ItemInstance inst{};
    assert(inst.instance_id == 0);
    assert(inst.definition_id == 0);
    assert(inst.perk_ids.empty());
    assert(inst.mod_ids.empty());
    std::cout << "test_item_instance_defaults passed.\n";
}

void test_item_instance_with_values() {
    ItemInstance inst{42, 2001, {1001, 1002}, {}};
    assert(inst.instance_id == 42);
    assert(inst.definition_id == 2001);
    assert(inst.perk_ids.size() == 2);
    assert(inst.perk_ids[0] == 1001);
    assert(inst.perk_ids[1] == 1002);
    assert(inst.mod_ids.empty());
    std::cout << "test_item_instance_with_values passed.\n";
}

// ===================================================================
//  ProgressionState
// ===================================================================

void test_progression_defaults() {
    ProgressionState prog{};
    assert(prog.level == 1);
    assert(prog.xp == 0);
    assert(prog.xp_to_next_level == 1000);
    std::cout << "test_progression_defaults passed.\n";
}

void test_progression_add_xp_no_level() {
    ProgressionState prog{};
    bool leveled = prog.add_xp(500);
    assert(!leveled);
    assert(prog.xp == 500);
    assert(prog.level == 1);
    std::cout << "test_progression_add_xp_no_level passed.\n";
}

void test_progression_add_xp_level_up() {
    ProgressionState prog{};
    bool leveled = prog.add_xp(1000);
    assert(leveled);
    assert(prog.xp == 0);
    assert(prog.level == 2);
    // Curve: 1000 * 1.15^(2-1) = 1150
    assert(prog.xp_to_next_level == 1150);
    std::cout << "test_progression_add_xp_level_up passed.\n";
}

void test_progression_add_xp_multi_level() {
    ProgressionState prog{};
    // Add enough XP to level up multiple times
    bool leveled = prog.add_xp(2500);  // 1000 + 1150 + 350
    assert(leveled);
    // Should have gained 2 levels (1000 → level 2, 1150 → level 3)
    assert(prog.level == 3);
    assert(prog.xp == 350);  // 2500 - 1000 - 1150
    // Curve: 1000 * 1.15^(3-1) = 1000 * 1.3225 = 1322 (truncated)
    assert(prog.xp_to_next_level == 1322);
    std::cout << "test_progression_add_xp_multi_level passed.\n";
}

void test_progression_set_level() {
    ProgressionState prog{};
    prog.set_level(5);
    assert(prog.level == 5);
    assert(prog.xp == 0);
    // Curve: 1000 * 1.15^(5-1) = 1000 * 1.749 = 1749
    assert(prog.xp_to_next_level == 1749);
    std::cout << "test_progression_set_level passed.\n";
}

void test_progression_set_level_zero() {
    ProgressionState prog{};
    prog.set_level(0);  // clamped to 1
    assert(prog.level == 1);
    assert(prog.xp_to_next_level == 1000);
    std::cout << "test_progression_set_level_zero passed.\n";
}

void test_progression_exact_boundary() {
    ProgressionState prog{};
    // Exactly at boundary — levels up
    assert(prog.add_xp(1000));
    assert(prog.level == 2);
    assert(prog.xp == 0);
    std::cout << "test_progression_exact_boundary passed.\n";
}

// ===================================================================
//  CurrencyState
// ===================================================================

void test_currency_defaults() {
    CurrencyState cur{};
    assert(cur.glimmer == 0);
    std::cout << "test_currency_defaults passed.\n";
}

void test_currency_earn() {
    CurrencyState cur{};
    cur.earn_glimmer(500);
    assert(cur.glimmer == 500);
    cur.earn_glimmer(250);
    assert(cur.glimmer == 750);
    std::cout << "test_currency_earn passed.\n";
}

void test_currency_spend_sufficient() {
    CurrencyState cur{};
    cur.earn_glimmer(1000);
    bool ok = cur.spend_glimmer(600);
    assert(ok);
    assert(cur.glimmer == 400);
    std::cout << "test_currency_spend_sufficient passed.\n";
}

void test_currency_spend_insufficient() {
    CurrencyState cur{};
    cur.earn_glimmer(100);
    bool ok = cur.spend_glimmer(200);
    assert(!ok);
    assert(cur.glimmer == 100);  // unchanged
    std::cout << "test_currency_spend_insufficient passed.\n";
}

void test_currency_spend_exact() {
    CurrencyState cur{};
    cur.earn_glimmer(500);
    assert(cur.spend_glimmer(500));
    assert(cur.glimmer == 0);
    std::cout << "test_currency_spend_exact passed.\n";
}

// ===================================================================
//  Inventory — Item Management
// ===================================================================

void test_inventory_initial_state() {
    Inventory inv{};
    assert(inv.item_count() == 0);
    assert(!inv.is_full());
    assert(inv.max_items() == 50);
    assert(inv.begin() == inv.end());
    std::cout << "test_inventory_initial_state passed.\n";
}

void test_inventory_add_item() {
    Inventory inv{};
    ae::u32 id = inv.add_item(2001);
    assert(id != 0);
    assert(inv.item_count() == 1);

    const ItemInstance* item = inv.get_item(id);
    assert(item != nullptr);
    assert(item->instance_id == id);
    assert(item->definition_id == 2001);
    assert(item->perk_ids.empty());
    assert(item->mod_ids.empty());
    std::cout << "test_inventory_add_item passed.\n";
}

void test_inventory_add_multiple_items() {
    Inventory inv{};
    ae::u32 id1 = inv.add_item(2001);
    ae::u32 id2 = inv.add_item(2002);
    ae::u32 id3 = inv.add_item(2003);
    assert(id1 != id2);
    assert(id2 != id3);
    assert(id1 != id3);
    assert(inv.item_count() == 3);

    assert(inv.get_item(id1) != nullptr);
    assert(inv.get_item(id2) != nullptr);
    assert(inv.get_item(id3) != nullptr);
    std::cout << "test_inventory_add_multiple_items passed.\n";
}

void test_inventory_remove_item() {
    Inventory inv{};
    ae::u32 id = inv.add_item(2001);
    assert(inv.item_count() == 1);

    bool ok = inv.remove_item(id);
    assert(ok);
    assert(inv.item_count() == 0);
    assert(inv.get_item(id) == nullptr);
    std::cout << "test_inventory_remove_item passed.\n";
}

void test_inventory_remove_nonexistent() {
    Inventory inv{};
    bool ok = inv.remove_item(9999);
    assert(!ok);
    std::cout << "test_inventory_remove_nonexistent passed.\n";
}

void test_inventory_get_nonexistent() {
    const Inventory inv{};
    assert(inv.get_item(9999) == nullptr);
    std::cout << "test_inventory_get_nonexistent passed.\n";
}

void test_inventory_max_items() {
    Inventory inv{};
    inv.set_max_items(3);
    assert(inv.max_items() == 3);

    assert(inv.add_item(2001) != 0);
    assert(inv.add_item(2002) != 0);
    assert(inv.add_item(2003) != 0);
    assert(inv.is_full());

    // Should fail — inventory is full
    ae::u32 id = inv.add_item(2004);
    assert(id == 0);
    assert(inv.item_count() == 3);
    std::cout << "test_inventory_max_items passed.\n";
}

void test_inventory_clear() {
    Inventory inv{};
    inv.add_item(2001);
    inv.add_item(2002);
    inv.add_item(2003);
    assert(inv.item_count() == 3);

    inv.clear();
    assert(inv.item_count() == 0);
    assert(inv.get_item(1) == nullptr);
    assert(inv.get_item(2) == nullptr);
    assert(inv.get_item(3) == nullptr);
    std::cout << "test_inventory_clear passed.\n";
}

void test_inventory_items_by_type() {
    // Register items so type lookup works
    auto& registry = ItemRegistry::instance();

    ItemDefinition pistol;
    pistol.definition_id = 9001;
    pistol.display_name = "Test Pistol";
    pistol.type = ItemType::Weapon;
    pistol.slot = ItemSlot::WeaponSecondary;
    registry.register_item(pistol);

    ItemDefinition helmet;
    helmet.definition_id = 9002;
    helmet.display_name = "Test Helmet";
    helmet.type = ItemType::Armor;
    helmet.slot = ItemSlot::ArmorHelmet;
    registry.register_item(helmet);

    Inventory inv{};
    ae::u32 w_id = inv.add_item(9001);
    ae::u32 a_id = inv.add_item(9002);

    auto weapons = inv.get_items_by_type(ItemType::Weapon);
    assert(weapons.size() == 1);
    assert(weapons[0]->instance_id == w_id);

    auto armors = inv.get_items_by_type(ItemType::Armor);
    assert(armors.size() == 1);
    assert(armors[0]->instance_id == a_id);

    auto perks = inv.get_items_by_type(ItemType::Perk);
    assert(perks.empty());

    registry.clear();
    std::cout << "test_inventory_items_by_type passed.\n";
}

void test_inventory_items_by_slot() {
    auto& registry = ItemRegistry::instance();

    ItemDefinition ar;
    ar.definition_id = 9011;
    ar.display_name = "Test AR";
    ar.type = ItemType::Weapon;
    ar.slot = ItemSlot::WeaponPrimary;
    registry.register_item(ar);

    ItemDefinition helmet;
    helmet.definition_id = 9012;
    helmet.display_name = "Test Helmet";
    helmet.type = ItemType::Armor;
    helmet.slot = ItemSlot::ArmorHelmet;
    registry.register_item(helmet);

    Inventory inv{};
    ae::u32 ar_id = inv.add_item(9011);
    inv.add_item(9012);

    auto primary_weps = inv.get_items_by_slot(ItemSlot::WeaponPrimary);
    assert(primary_weps.size() == 1);
    assert(primary_weps[0]->instance_id == ar_id);

    registry.clear();
    std::cout << "test_inventory_items_by_slot passed.\n";
}

// ===================================================================
//  Inventory — Equipment
// ===================================================================

void test_inventory_equip_item() {
    Inventory inv{};
    ae::u32 id = inv.add_item(2001);

    bool ok = inv.equip_item(id, ItemSlot::WeaponPrimary);
    assert(ok);

    const ItemInstance* equipped = inv.get_equipped(ItemSlot::WeaponPrimary);
    assert(equipped != nullptr);
    assert(equipped->instance_id == id);

    assert(inv.get_equipped_id(ItemSlot::WeaponPrimary) == id);
    std::cout << "test_inventory_equip_item passed.\n";
}

void test_inventory_equip_nonexistent() {
    Inventory inv{};
    bool ok = inv.equip_item(9999, ItemSlot::WeaponPrimary);
    assert(!ok);
    std::cout << "test_inventory_equip_nonexistent passed.\n";
}

void test_inventory_equip_invalid_slot() {
    Inventory inv{};
    ae::u32 id = inv.add_item(2001);
    // Use ItemSlot::None which is beyond valid range
    bool ok = inv.equip_item(id, static_cast<ItemSlot>(99));
    assert(!ok);
    std::cout << "test_inventory_equip_invalid_slot passed.\n";
}

void test_inventory_equip_displaces() {
    Inventory inv{};
    ae::u32 id1 = inv.add_item(2001);
    ae::u32 id2 = inv.add_item(2002);

    inv.equip_item(id1, ItemSlot::WeaponPrimary);
    assert(inv.get_equipped_id(ItemSlot::WeaponPrimary) == id1);

    // Equip id2 into same slot — displaces id1
    inv.equip_item(id2, ItemSlot::WeaponPrimary);
    assert(inv.get_equipped_id(ItemSlot::WeaponPrimary) == id2);

    // id1 should still be in inventory
    assert(inv.get_item(id1) != nullptr);
    std::cout << "test_inventory_equip_displaces passed.\n";
}

void test_inventory_unequip() {
    Inventory inv{};
    ae::u32 id = inv.add_item(2001);
    inv.equip_item(id, ItemSlot::WeaponPrimary);
    assert(inv.get_equipped(ItemSlot::WeaponPrimary) != nullptr);

    bool ok = inv.unequip_item(ItemSlot::WeaponPrimary);
    assert(ok);
    assert(inv.get_equipped(ItemSlot::WeaponPrimary) == nullptr);
    assert(inv.get_equipped_id(ItemSlot::WeaponPrimary) == 0);

    // Item still in inventory
    assert(inv.get_item(id) != nullptr);
    std::cout << "test_inventory_unequip passed.\n";
}

void test_inventory_unequip_invalid_slot() {
    Inventory inv{};
    bool ok = inv.unequip_item(static_cast<ItemSlot>(99));
    assert(!ok);
    std::cout << "test_inventory_unequip_invalid_slot passed.\n";
}

void test_inventory_equip_remove_unequips() {
    Inventory inv{};
    ae::u32 id = inv.add_item(2001);
    inv.equip_item(id, ItemSlot::WeaponPrimary);
    assert(inv.get_equipped(ItemSlot::WeaponPrimary) != nullptr);

    inv.remove_item(id);
    assert(inv.get_equipped(ItemSlot::WeaponPrimary) == nullptr);
    std::cout << "test_inventory_equip_remove_unequips passed.\n";
}

void test_inventory_equip_multiple_slots() {
    Inventory inv{};
    ae::u32 weapon_id = inv.add_item(2001);
    ae::u32 helmet_id = inv.add_item(3001);

    inv.equip_item(weapon_id, ItemSlot::WeaponPrimary);
    inv.equip_item(helmet_id, ItemSlot::ArmorHelmet);

    assert(inv.get_equipped_id(ItemSlot::WeaponPrimary) == weapon_id);
    assert(inv.get_equipped_id(ItemSlot::ArmorHelmet) == helmet_id);
    std::cout << "test_inventory_equip_multiple_slots passed.\n";
}

// ===================================================================
//  Inventory — Perks & Mods
// ===================================================================

void test_inventory_attach_perk() {
    Inventory inv{};
    ae::u32 id = inv.add_item(2001);

    bool ok = inv.attach_perk(id, 1001);
    assert(ok);

    const ItemInstance* item = inv.get_item(id);
    assert(item->perk_ids.size() == 1);
    assert(item->perk_ids[0] == 1001);
    std::cout << "test_inventory_attach_perk passed.\n";
}

void test_inventory_attach_perk_duplicate() {
    Inventory inv{};
    ae::u32 id = inv.add_item(2001);
    inv.attach_perk(id, 1001);

    bool ok = inv.attach_perk(id, 1001);
    assert(!ok);  // duplicate

    assert(inv.get_item(id)->perk_ids.size() == 1);
    std::cout << "test_inventory_attach_perk_duplicate passed.\n";
}

void test_inventory_attach_perk_nonexistent_item() {
    Inventory inv{};
    bool ok = inv.attach_perk(9999, 1001);
    assert(!ok);
    std::cout << "test_inventory_attach_perk_nonexistent_item passed.\n";
}

void test_inventory_remove_perk() {
    Inventory inv{};
    ae::u32 id = inv.add_item(2001);
    inv.attach_perk(id, 1001);
    inv.attach_perk(id, 1002);
    assert(inv.get_item(id)->perk_ids.size() == 2);

    bool ok = inv.remove_perk(id, 1001);
    assert(ok);

    const ItemInstance* item = inv.get_item(id);
    assert(item->perk_ids.size() == 1);
    assert(item->perk_ids[0] == 1002);
    std::cout << "test_inventory_remove_perk passed.\n";
}

void test_inventory_remove_perk_nonexistent() {
    Inventory inv{};
    ae::u32 id = inv.add_item(2001);

    bool ok = inv.remove_perk(id, 9999);
    assert(!ok);
    std::cout << "test_inventory_remove_perk_nonexistent passed.\n";
}

void test_inventory_attach_mod() {
    Inventory inv{};
    ae::u32 id = inv.add_item(2001);

    bool ok = inv.attach_mod(id, 5001);
    assert(ok);

    const ItemInstance* item = inv.get_item(id);
    assert(item->mod_ids.size() == 1);
    assert(item->mod_ids[0] == 5001);
    std::cout << "test_inventory_attach_mod passed.\n";
}

void test_inventory_attach_mod_duplicate() {
    Inventory inv{};
    ae::u32 id = inv.add_item(2001);
    inv.attach_mod(id, 5001);

    bool ok = inv.attach_mod(id, 5001);
    assert(!ok);
    assert(inv.get_item(id)->mod_ids.size() == 1);
    std::cout << "test_inventory_attach_mod_duplicate passed.\n";
}

void test_inventory_remove_mod() {
    Inventory inv{};
    ae::u32 id = inv.add_item(2001);
    inv.attach_mod(id, 5001);
    inv.attach_mod(id, 5002);
    assert(inv.get_item(id)->mod_ids.size() == 2);

    bool ok = inv.remove_mod(id, 5001);
    assert(ok);

    const ItemInstance* item = inv.get_item(id);
    assert(item->mod_ids.size() == 1);
    assert(item->mod_ids[0] == 5002);
    std::cout << "test_inventory_remove_mod passed.\n";
}

void test_inventory_perks_and_mods_independent() {
    Inventory inv{};
    ae::u32 id = inv.add_item(2001);

    inv.attach_perk(id, 1001);
    inv.attach_mod(id, 5001);

    const ItemInstance* item = inv.get_item(id);
    assert(item->perk_ids.size() == 1);
    assert(item->mod_ids.size() == 1);

    inv.remove_perk(id, 1001);
    assert(item->perk_ids.empty());
    assert(item->mod_ids.size() == 1);  // mods unaffected

    std::cout << "test_inventory_perks_and_mods_independent passed.\n";
}

// ===================================================================
//  Inventory — Iteration
// ===================================================================

void test_inventory_iteration() {
    Inventory inv{};
    const ae::u32 id1 = inv.add_item(2001);
    const ae::u32 id2 = inv.add_item(2002);
    const ae::u32 id3 = inv.add_item(2003);

    bool found1 = false, found2 = false, found3 = false;
    int count = 0;
    for (auto it = inv.begin(); it != inv.end(); ++it) {
        if (it->second.instance_id == id1) found1 = true;
        if (it->second.instance_id == id2) found2 = true;
        if (it->second.instance_id == id3) found3 = true;
        ++count;
    }
    assert(count == 3);
    assert(found1 && found2 && found3);
    std::cout << "test_inventory_iteration passed.\n";
}

void test_inventory_empty_iteration() {
    const Inventory inv{};
    assert(inv.begin() == inv.end());
    std::cout << "test_inventory_empty_iteration passed.\n";
}

}  // namespace

int main() {
    // ItemInstance
    test_item_instance_defaults();
    test_item_instance_with_values();

    // ProgressionState
    test_progression_defaults();
    test_progression_add_xp_no_level();
    test_progression_add_xp_level_up();
    test_progression_add_xp_multi_level();
    test_progression_set_level();
    test_progression_set_level_zero();
    test_progression_exact_boundary();

    // CurrencyState
    test_currency_defaults();
    test_currency_earn();
    test_currency_spend_sufficient();
    test_currency_spend_insufficient();
    test_currency_spend_exact();

    // Inventory — Item Management
    test_inventory_initial_state();
    test_inventory_add_item();
    test_inventory_add_multiple_items();
    test_inventory_remove_item();
    test_inventory_remove_nonexistent();
    test_inventory_get_nonexistent();
    test_inventory_max_items();
    test_inventory_clear();
    test_inventory_items_by_type();
    test_inventory_items_by_slot();

    // Inventory — Equipment
    test_inventory_equip_item();
    test_inventory_equip_nonexistent();
    test_inventory_equip_invalid_slot();
    test_inventory_equip_displaces();
    test_inventory_unequip();
    test_inventory_unequip_invalid_slot();
    test_inventory_equip_remove_unequips();
    test_inventory_equip_multiple_slots();

    // Inventory — Perks & Mods
    test_inventory_attach_perk();
    test_inventory_attach_perk_duplicate();
    test_inventory_attach_perk_nonexistent_item();
    test_inventory_remove_perk();
    test_inventory_remove_perk_nonexistent();
    test_inventory_attach_mod();
    test_inventory_attach_mod_duplicate();
    test_inventory_remove_mod();
    test_inventory_perks_and_mods_independent();

    // Inventory — Iteration
    test_inventory_iteration();
    test_inventory_empty_iteration();

    std::cout << "\nAll inventory tests passed.\n";
    return 0;
}
