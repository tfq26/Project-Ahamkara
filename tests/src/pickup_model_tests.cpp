#include "ahamkara/game/pickup_model.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

namespace {

using namespace ahamkara::game;

// ===================================================================
// GrantId tests
// ===================================================================

void test_grant_id_default() {
    GrantId id{};
    assert(id.source_id == 0);
    assert(id.play_session_id == 0);
    assert(id.sequence == 0);
    std::cout << "test_grant_id_default passed.\n";
}

void test_grant_id_to_string_roundtrip() {
    GrantId id;
    id.source_id = 0xDEADBEEFCAFE;
    id.play_session_id = 1001;
    id.sequence = 42;

    std::string str = id.to_string();
    GrantId parsed = GrantId::from_string(str);

    assert(parsed.source_id == id.source_id);
    assert(parsed.play_session_id == id.play_session_id);
    assert(parsed.sequence == id.sequence);
    std::cout << "test_grant_id_to_string_roundtrip passed.\n";
}

void test_grant_id_from_string_malformed() {
    GrantId parsed = GrantId::from_string("not-a-valid-grant-id");
    assert(parsed.source_id == 0);
    assert(parsed.play_session_id == 0);
    assert(parsed.sequence == 0);
    std::cout << "test_grant_id_from_string_malformed passed.\n";
}

void test_grant_id_equality() {
    GrantId a{};
    a.source_id = 10;
    a.play_session_id = 5;
    a.sequence = 3;

    GrantId b = a;
    assert(a == b);

    b.sequence = 4;
    assert(a != b);
    std::cout << "test_grant_id_equality passed.\n";
}

void test_grant_id_from_string_empty() {
    GrantId parsed = GrantId::from_string("");
    assert(parsed.source_id == 0);
    std::cout << "test_grant_id_from_string_empty passed.\n";
}

// ===================================================================
// RewardGrant tests
// ===================================================================

void test_reward_grant_valid() {
    RewardGrant grant;
    grant.id.sequence = 1;
    grant.type = PickupType::PrimaryAmmo;
    grant.quantity = 30.0F;
    assert(grant.is_valid());
    std::cout << "test_reward_grant_valid passed.\n";
}

void test_reward_grant_invalid_type() {
    RewardGrant grant;
    grant.type = PickupType::Count;  // sentinel — not a real pickup
    grant.quantity = 10.0F;
    assert(!grant.is_valid());
    std::cout << "test_reward_grant_invalid_type passed.\n";
}

void test_reward_grant_zero_quantity() {
    RewardGrant grant;
    grant.type = PickupType::PrimaryAmmo;
    grant.quantity = 0.0F;
    assert(!grant.is_valid());
    std::cout << "test_reward_grant_zero_quantity passed.\n";
}

void test_reward_grant_negative_quantity() {
    RewardGrant grant;
    grant.type = PickupType::PrimaryAmmo;
    grant.quantity = -1.0F;
    assert(!grant.is_valid());
    std::cout << "test_reward_grant_negative_quantity passed.\n";
}

// ===================================================================
// Inventory tests
// ===================================================================

void test_inventory_default_state() {
    Inventory inv{};
    assert(inv.weapons().empty());
    assert(inv.armor().empty());
    assert(inv.perks().empty());
    assert(inv.current_health() == 100.0F);
    assert(inv.max_health() == 100.0F);
    assert(inv.current_shield() == 50.0F);
    assert(inv.max_shield() == 100.0F);
    assert(inv.grenade_count() == 2);
    assert(inv.xp() == 0);
    assert(inv.currency() == 0);
    std::cout << "test_inventory_default_state passed.\n";
}

void test_inventory_reset() {
    Inventory inv{};
    inv.set_current_health(50.0F);
    inv.set_currency(999);
    inv.add_weapon({1, 1, false, 100});

    inv.reset();
    assert(inv.weapons().empty());
    assert(inv.current_health() == 100.0F);
    assert(inv.currency() == 0);
    std::cout << "test_inventory_reset passed.\n";
}

void test_inventory_weapon_management() {
    Inventory inv{};

    // Add weapons until full
    for (ae::u32 i = 1; i <= InventoryLimits::kMaxWeaponSlots; ++i) {
        InventoryEntry entry;
        entry.item_definition_id = i;
        entry.instance_id = i;
        assert(inv.add_weapon(entry));
    }

    // Should be full
    assert(inv.weapons_full());
    assert(!inv.add_weapon({11, 1, false, 999}));

    // Check containment
    assert(inv.has_weapon(1));
    assert(inv.has_weapon(InventoryLimits::kMaxWeaponSlots));
    assert(!inv.has_weapon(999));

    // Remove
    assert(inv.remove_weapon(1));
    assert(!inv.has_weapon(1));
    assert(!inv.weapons_full());  // now one free slot
    std::cout << "test_inventory_weapon_management passed.\n";
}

void test_inventory_armor_management() {
    Inventory inv{};

    for (ae::u32 i = 1; i <= InventoryLimits::kMaxArmorSlots; ++i) {
        InventoryEntry entry;
        entry.item_definition_id = i;
        entry.instance_id = i;
        assert(inv.add_armor(entry));
    }

    assert(inv.armor_full());
    assert(!inv.add_armor({99, 1, false, 999}));
    assert(inv.has_armor(1));
    assert(!inv.has_armor(99));

    assert(inv.remove_armor(1));
    assert(!inv.has_armor(1));
    std::cout << "test_inventory_armor_management passed.\n";
}

void test_inventory_perk_management() {
    Inventory inv{};

    for (ae::u32 i = 1; i <= InventoryLimits::kMaxPerks; ++i) {
        InventoryEntry entry;
        entry.item_definition_id = i;
        entry.instance_id = i;
        assert(inv.add_perk(entry));
    }

    assert(inv.perks_full());
    assert(!inv.add_perk({999, 1, false, 555}));
    assert(inv.has_perk(1));
    assert(!inv.has_perk(999));
    std::cout << "test_inventory_perk_management passed.\n";
}

void test_inventory_resource_caps() {
    Inventory inv{};

    // Ammo caps at max
    inv.set_primary_ammo(500);  // above max of 300
    assert(inv.primary_ammo() == 500);  // setter doesn't clamp (it's a setter)
    // The clamping is done by the processor, not the setter

    // Negative protection
    inv.set_primary_ammo(-10);
    assert(inv.primary_ammo() == 0);  // setter clamps negative

    // Grenade cap
    inv.set_grenade_count(5);
    assert(inv.grenade_count() == 5);  // setter doesn't clamp high either

    std::cout << "test_inventory_resource_caps passed.\n";
}

// ===================================================================
// PickupProcessor tests
// ===================================================================

void test_processor_grant_ammo() {
    Inventory inv{};
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::PrimaryAmmo;
    grant.quantity = 30.0F;

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::Granted);
    assert(inv.primary_ammo() == 30);

    std::cout << "test_processor_grant_ammo passed.\n";
}

void test_processor_ammo_at_cap() {
    Inventory inv{};
    inv.set_primary_ammo(300);  // already at max
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::PrimaryAmmo;
    grant.quantity = 30.0F;

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::AtCap);
    assert(inv.primary_ammo() == 300);  // unchanged

    std::cout << "test_processor_ammo_at_cap passed.\n";
}

void test_processor_ammo_partial_cap() {
    Inventory inv{};
    inv.set_primary_ammo(290);  // 10 from max
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::PrimaryAmmo;
    grant.quantity = 30.0F;

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::Granted);
    assert(inv.primary_ammo() == 300);  // clamped to max

    std::cout << "test_processor_ammo_partial_cap passed.\n";
}

void test_processor_all_ammo_types() {
    struct TestCase {
        PickupType type;
        int Inventory::*getter;
        int Inventory::*max_getter;
        int set_value;
        int grant_qty;
    };

    // We'll test each ammo type directly
    {
        Inventory inv{};
        PickupProcessor proc{};
        RewardGrant g{{1, 1, 1}, PickupType::PrimaryAmmo, 30};
        assert(proc.process_grant(inv, g) == GrantResult::Granted);
        assert(inv.primary_ammo() == 30);
    }
    {
        Inventory inv{};
        PickupProcessor proc{};
        RewardGrant g{{2, 1, 1}, PickupType::SecondaryAmmo, 25};
        assert(proc.process_grant(inv, g) == GrantResult::Granted);
        assert(inv.secondary_ammo() == 25);
    }
    {
        Inventory inv{};
        PickupProcessor proc{};
        RewardGrant g{{3, 1, 1}, PickupType::HeavyAmmo, 5};
        assert(proc.process_grant(inv, g) == GrantResult::Granted);
        assert(inv.heavy_ammo() == 5);
    }
    {
        Inventory inv{};
        PickupProcessor proc{};
        RewardGrant g{{4, 1, 1}, PickupType::SpecialAmmo, 10};
        assert(proc.process_grant(inv, g) == GrantResult::Granted);
        assert(inv.special_ammo() == 10);
    }

    std::cout << "test_processor_all_ammo_types passed.\n";
}

void test_processor_grant_health() {
    Inventory inv{};
    inv.set_current_health(70.0F);
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::HealthPack;
    grant.quantity = 20.0F;

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::Granted);
    assert(inv.current_health() == 90.0F);

    std::cout << "test_processor_grant_health passed.\n";
}

void test_processor_health_full() {
    Inventory inv{};
    inv.set_current_health(100.0F);
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::HealthPack;
    grant.quantity = 20.0F;

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::AtCap);
    assert(inv.current_health() == 100.0F);

    std::cout << "test_processor_health_full passed.\n";
}

void test_processor_health_partial() {
    Inventory inv{};
    inv.set_current_health(95.0F);
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::LargeHealth;  // large health
    grant.quantity = 20.0F;

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::Granted);
    assert(inv.current_health() == 100.0F);  // clamped

    std::cout << "test_processor_health_partial passed.\n";
}

void test_processor_grant_shield() {
    Inventory inv{};
    inv.set_current_shield(20.0F);
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::ShieldCharge;
    grant.quantity = 30.0F;

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::Granted);
    assert(inv.current_shield() == 50.0F);

    std::cout << "test_processor_grant_shield passed.\n";
}

void test_processor_shield_full() {
    Inventory inv{};
    inv.set_current_shield(100.0F);
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::ShieldCharge;
    grant.quantity = 30.0F;

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::AtCap);

    std::cout << "test_processor_shield_full passed.\n";
}

void test_processor_grant_weapon_drop() {
    Inventory inv{};
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::WeaponDrop;
    grant.quantity = 1.0F;
    grant.item_def_id = 100;  // e.g. "Auto Rifle"

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::Granted);
    assert(inv.has_weapon(100));
    assert(inv.weapons().size() == 1);

    std::cout << "test_processor_grant_weapon_drop passed.\n";
}

void test_processor_weapon_drop_duplicate() {
    Inventory inv{};
    PickupProcessor proc{};

    // First drop — should grant
    {
        RewardGrant grant;
        grant.id = {1, 1, 1};
        grant.type = PickupType::WeaponDrop;
        grant.quantity = 1.0F;
        grant.item_def_id = 100;
        assert(proc.process_grant(inv, grant) == GrantResult::Granted);
    }

    // Second drop of same weapon — should be duplicate
    {
        RewardGrant grant;
        grant.id = {1, 1, 2};  // different grant ID, but same weapon
        grant.type = PickupType::WeaponDrop;
        grant.quantity = 1.0F;
        grant.item_def_id = 100;
        assert(proc.process_grant(inv, grant) == GrantResult::Duplicate);
    }

    std::cout << "test_processor_weapon_drop_duplicate passed.\n";
}

void test_processor_weapon_drop_inventory_full() {
    Inventory inv{};
    PickupProcessor proc{};

    // Fill weapon inventory
    for (ae::u32 i = 1; i <= InventoryLimits::kMaxWeaponSlots; ++i) {
        InventoryEntry entry;
        entry.item_definition_id = i;
        entry.instance_id = i;
        inv.add_weapon(entry);
    }

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::WeaponDrop;
    grant.quantity = 1.0F;
    grant.item_def_id = 999;

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::InventoryFull);

    std::cout << "test_processor_weapon_drop_inventory_full passed.\n";
}

void test_processor_grant_armor_drop() {
    Inventory inv{};
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::ArmorDrop;
    grant.quantity = 1.0F;
    grant.item_def_id = 200;

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::Granted);
    assert(inv.has_armor(200));

    std::cout << "test_processor_grant_armor_drop passed.\n";
}

void test_processor_grant_perk_drop() {
    Inventory inv{};
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::PerkDrop;
    grant.quantity = 1.0F;
    grant.item_def_id = 300;

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::Granted);
    assert(inv.has_perk(300));

    std::cout << "test_processor_grant_perk_drop passed.\n";
}

void test_processor_grenade_charge() {
    Inventory inv{};
    inv.set_grenade_count(1);  // one grenade consumed
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::GrenadeCharge;
    grant.quantity = 1.0F;

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::Granted);
    assert(inv.grenade_count() == 2);

    std::cout << "test_processor_grenade_charge passed.\n";
}

void test_processor_grenade_at_cap() {
    Inventory inv{};
    inv.set_grenade_count(2);  // already at cap
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::GrenadeCharge;
    grant.quantity = 1.0F;

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::AtCap);

    std::cout << "test_processor_grenade_at_cap passed.\n";
}

void test_processor_ultimate_energy() {
    Inventory inv{};
    inv.set_ultimate_charge(0.5F);
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::UltimateEnergy;
    grant.quantity = 0.3F;

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::Granted);
    assert(inv.ultimate_charge() >= 0.79F && inv.ultimate_charge() <= 0.81F);

    std::cout << "test_processor_ultimate_energy passed.\n";
}

void test_processor_ultimate_at_cap() {
    Inventory inv{};
    inv.set_ultimate_charge(1.0F);
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::UltimateEnergy;
    grant.quantity = 0.3F;

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::AtCap);

    std::cout << "test_processor_ultimate_at_cap passed.\n";
}

void test_processor_xp() {
    Inventory inv{};
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::XPOrb;
    grant.quantity = 100.0F;

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::Granted);
    assert(inv.xp() == 100);

    // Accumulate
    RewardGrant grant2;
    grant2.id = {1, 1, 2};
    grant2.type = PickupType::XPOrb;
    grant2.quantity = 50.0F;
    assert(proc.process_grant(inv, grant2) == GrantResult::Granted);
    assert(inv.xp() == 150);

    std::cout << "test_processor_xp passed.\n";
}

void test_processor_currency() {
    Inventory inv{};
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::Currency;
    grant.quantity = 500.0F;

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::Granted);
    assert(inv.currency() == 500);

    std::cout << "test_processor_currency passed.\n";
}

void test_processor_currency_overflow_saturation() {
    Inventory inv{};
    inv.set_currency(std::numeric_limits<ae::u64>::max() - 10);
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::Currency;
    grant.quantity = 20.0F;

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::Granted);
    // Should saturate at max
    assert(inv.currency() == std::numeric_limits<ae::u64>::max());

    std::cout << "test_processor_currency_overflow_saturation passed.\n";
}

// ===================================================================
// Idempotency tests
// ===================================================================

void test_processor_idempotency_duplicate_grant() {
    Inventory inv{};
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::PrimaryAmmo;
    grant.quantity = 30.0F;

    // First grant — should succeed
    assert(proc.process_grant(inv, grant) == GrantResult::Granted);
    assert(inv.primary_ammo() == 30);

    // Same grant ID again — should return Duplicate
    assert(proc.process_grant(inv, grant) == GrantResult::Duplicate);
    assert(inv.primary_ammo() == 30);  // unchanged

    std::cout << "test_processor_idempotency_duplicate_grant passed.\n";
}

void test_processor_idempotency_different_grants() {
    Inventory inv{};
    PickupProcessor proc{};

    RewardGrant grant1;
    grant1.id = {1, 1, 1};
    grant1.type = PickupType::XPOrb;
    grant1.quantity = 100.0F;

    RewardGrant grant2;
    grant2.id = {1, 1, 2};  // different sequence
    grant2.type = PickupType::XPOrb;
    grant2.quantity = 50.0F;

    assert(proc.process_grant(inv, grant1) == GrantResult::Granted);
    assert(proc.process_grant(inv, grant2) == GrantResult::Granted);
    assert(inv.xp() == 150);

    std::cout << "test_processor_idempotency_different_grants passed.\n";
}

void test_processor_reset_idempotency() {
    Inventory inv{};
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::PrimaryAmmo;
    grant.quantity = 30.0F;

    assert(proc.process_grant(inv, grant) == GrantResult::Granted);
    assert(proc.process_grant(inv, grant) == GrantResult::Duplicate);

    proc.reset_idempotency();
    assert(!proc.has_processed(grant.id));
    // After reset, should re-grant (inventory has 30, cap is 300)
    assert(proc.process_grant(inv, grant) == GrantResult::Granted);
    assert(inv.primary_ammo() == 60);

    std::cout << "test_processor_reset_idempotency passed.\n";
}

void test_processor_prune_grants_before_session() {
    Inventory inv{};
    PickupProcessor proc{};

    // Grant from session 1
    RewardGrant grant1{{1, 1, 1}, PickupType::XPOrb, 100};
    assert(proc.process_grant(inv, grant1) == GrantResult::Granted);

    // Grant from session 2
    RewardGrant grant2{{2, 2, 1}, PickupType::XPOrb, 50};
    assert(proc.process_grant(inv, grant2) == GrantResult::Granted);

    assert(inv.xp() == 150);

    // Prune session 1 grants
    proc.prune_grants_before_session(2);
    assert(!proc.has_processed(grant1.id));
    assert(proc.has_processed(grant2.id));

    std::cout << "test_processor_prune_grants_before_session passed.\n";
}

// ===================================================================
// Invalid grant tests
// ===================================================================

void test_processor_invalid_grant() {
    Inventory inv{};
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::Count;  // not a valid type
    grant.quantity = 10.0F;

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::Invalid);

    std::cout << "test_processor_invalid_grant passed.\n";
}

void test_processor_zero_quantity_grant() {
    Inventory inv{};
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::PrimaryAmmo;
    grant.quantity = 0.0F;

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::Invalid);

    std::cout << "test_processor_zero_quantity_grant passed.\n";
}

void test_processor_negative_quantity_grant() {
    Inventory inv{};
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::HealthPack;
    grant.quantity = -10.0F;

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::Invalid);

    std::cout << "test_processor_negative_quantity_grant passed.\n";
}

void test_processor_weapon_drop_zero_def_id() {
    Inventory inv{};
    PickupProcessor proc{};

    RewardGrant grant;
    grant.id = {1, 1, 1};
    grant.type = PickupType::WeaponDrop;
    grant.item_def_id = 0;  // invalid

    GrantResult result = proc.process_grant(inv, grant);
    assert(result == GrantResult::Invalid);

    std::cout << "test_processor_weapon_drop_zero_def_id passed.\n";
}

// ===================================================================
// ProgressionSnapshot tests
// ===================================================================

void test_snapshot_create_default() {
    ProgressionSnapshot snap = ProgressionSnapshot::create_default();
    assert(snap.version == ProgressionSnapshot::kCurrentVersion);
    assert(snap.xp == 0);
    assert(snap.currency == 0);
    assert(snap.weapons.empty());
    assert(snap.armor.empty());
    assert(snap.perks.empty());
    assert(snap.processed_grants.empty());
    assert(snap.validate());
    std::cout << "test_snapshot_create_default passed.\n";
}

void test_snapshot_validate_valid() {
    ProgressionSnapshot snap = ProgressionSnapshot::create_default();
    assert(snap.validate());

    // Add some weapons
    snap.weapons.push_back({1, 1, false, 100});
    snap.weapons.push_back({2, 1, true, 101});
    assert(snap.validate());

    snap.xp = 5000;
    snap.currency = 250;
    assert(snap.validate());

    std::cout << "test_snapshot_validate_valid passed.\n";
}

void test_snapshot_validate_invalid_version() {
    ProgressionSnapshot snap = ProgressionSnapshot::create_default();
    snap.version = 0;  // below min supported
    assert(!snap.validate());

    snap.version = 999;  // above current
    assert(!snap.validate());
    std::cout << "test_snapshot_validate_invalid_version passed.\n";
}

void test_snapshot_validate_oversized_collections() {
    ProgressionSnapshot snap = ProgressionSnapshot::create_default();

    // Fill weapons beyond capacity
    for (ae::u32 i = 0; i <= InventoryLimits::kMaxWeaponSlots; ++i) {
        snap.weapons.push_back({i + 1, 1, false, i});
    }
    assert(!snap.validate());

    std::cout << "test_snapshot_validate_oversized_collections passed.\n";
}

void test_snapshot_validate_zero_def_id() {
    ProgressionSnapshot snap = ProgressionSnapshot::create_default();
    snap.weapons.push_back({0, 1, false, 100});  // definition_id == 0
    assert(!snap.validate());
    std::cout << "test_snapshot_validate_zero_def_id passed.\n";
}

void test_snapshot_migrate_current() {
    ProgressionSnapshot snap = ProgressionSnapshot::create_default();
    snap.xp = 1000;
    snap.currency = 500;

    ProgressionSnapshot migrated = ProgressionSnapshot::migrate(snap);
    assert(migrated.version == ProgressionSnapshot::kCurrentVersion);
    assert(migrated.xp == 1000);  // data preserved
    assert(migrated.currency == 500);
    std::cout << "test_snapshot_migrate_current passed.\n";
}

void test_snapshot_migrate_unknown_version() {
    ProgressionSnapshot snap = ProgressionSnapshot::create_default();
    snap.version = 999;  // unknown / future
    snap.xp = 5000;

    ProgressionSnapshot migrated = ProgressionSnapshot::migrate(snap);
    // Should return default for unknown versions
    assert(migrated.xp == 0);
    std::cout << "test_snapshot_migrate_unknown_version passed.\n";
}

void test_snapshot_repair_negative_values() {
    ProgressionSnapshot snap = ProgressionSnapshot::create_default();
    snap.max_health = -100.0F;
    snap.max_shield = -50.0F;
    snap.max_primary_ammo = -10;

    ProgressionSnapshot repaired = ProgressionSnapshot::repair(snap);
    assert(repaired.max_health == 100.0F);   // reset to default
    assert(repaired.max_shield == 100.0F);   // reset to default
    assert(repaired.max_primary_ammo == 0);  // clamped to 0
    assert(repaired.validate());
    std::cout << "test_snapshot_repair_negative_values passed.\n";
}

void test_snapshot_repair_oversized_collections() {
    ProgressionSnapshot snap = ProgressionSnapshot::create_default();

    for (ae::u32 i = 0; i < InventoryLimits::kMaxWeaponSlots + 5; ++i) {
        snap.weapons.push_back({i + 1, 1, false, i});
    }
    assert(snap.weapons.size() > InventoryLimits::kMaxWeaponSlots);

    ProgressionSnapshot repaired = ProgressionSnapshot::repair(snap);
    assert(repaired.weapons.size() <= InventoryLimits::kMaxWeaponSlots);
    assert(repaired.validate());
    std::cout << "test_snapshot_repair_oversized_collections passed.\n";
}

void test_snapshot_repair_zero_def_ids() {
    ProgressionSnapshot snap = ProgressionSnapshot::create_default();
    snap.weapons.push_back({0, 1, false, 100});  // zero def id — invalid
    snap.weapons.push_back({1, 1, false, 101});
    snap.weapons.push_back({0, 1, false, 102});  // another invalid

    ProgressionSnapshot repaired = ProgressionSnapshot::repair(snap);
    assert(repaired.weapons.size() == 1);
    assert(repaired.weapons[0].item_definition_id == 1);
    assert(repaired.validate());
    std::cout << "test_snapshot_repair_zero_def_ids passed.\n";
}

void test_snapshot_repair_unknown_version() {
    ProgressionSnapshot snap = ProgressionSnapshot::create_default();
    snap.version = 0;  // below min supported
    snap.xp = 5000;

    ProgressionSnapshot repaired = ProgressionSnapshot::repair(snap);
    assert(repaired.version == ProgressionSnapshot::kCurrentVersion);
    assert(repaired.xp == 0);  // data lost — reset to default
    assert(repaired.validate());
    std::cout << "test_snapshot_repair_unknown_version passed.\n";
}

void test_snapshot_repair_excessive_values() {
    ProgressionSnapshot snap = ProgressionSnapshot::create_default();
    snap.max_health = 999999.0F;  // excessive
    snap.max_primary_ammo = 99999;

    ProgressionSnapshot repaired = ProgressionSnapshot::repair(snap);
    assert(repaired.max_health <= 10000.0F);
    assert(repaired.max_primary_ammo <= 10000);
    assert(repaired.validate());
    std::cout << "test_snapshot_repair_excessive_values passed.\n";
}

// ===================================================================
// Serialization tests
// ===================================================================

void test_serialization_roundtrip_default() {
    ProgressionSnapshot original = ProgressionSnapshot::create_default();
    std::vector<std::uint8_t> data = original.serialize();

    assert(!data.empty());

    ProgressionSnapshot restored = ProgressionSnapshot::deserialize(data.data(), data.size());
    assert(restored.version == original.version);
    assert(restored.xp == original.xp);
    assert(restored.currency == original.currency);
    assert(restored.weapons.size() == original.weapons.size());
    assert(restored.validate());

    std::cout << "test_serialization_roundtrip_default passed.\n";
}

void test_serialization_roundtrip_with_data() {
    ProgressionSnapshot snap = ProgressionSnapshot::create_default();
    snap.total_play_time_seconds = 3600;  // 1 hour
    snap.last_session_id = 42;
    snap.xp = 15000;
    snap.currency = 999;
    snap.max_health = 150.0F;
    snap.max_shield = 125.0F;

    snap.weapons.push_back({1, 1, true, 1001});
    snap.weapons.push_back({2, 1, false, 1002});
    snap.armor.push_back({10, 1, true, 2001});
    snap.perks.push_back({20, 1, false, 3001});

    snap.processed_grants.push_back({100, 42, 1});
    snap.processed_grants.push_back({100, 42, 2});

    std::vector<std::uint8_t> data = snap.serialize();
    ProgressionSnapshot restored = ProgressionSnapshot::deserialize(data.data(), data.size());

    assert(restored.total_play_time_seconds == snap.total_play_time_seconds);
    assert(restored.last_session_id == snap.last_session_id);
    assert(restored.xp == snap.xp);
    assert(restored.currency == snap.currency);
    assert(restored.max_health == snap.max_health);
    assert(restored.weapons.size() == 2);
    assert(restored.armor.size() == 1);
    assert(restored.perks.size() == 1);
    assert(restored.processed_grants.size() == 2);
    assert(restored.weapons[0].item_definition_id == 1);
    assert(restored.weapons[0].instance_id == 1001);
    assert(restored.weapons[1].item_definition_id == 2);
    assert(restored.processed_grants[0].source_id == 100);
    assert(restored.processed_grants[1].sequence == 2);
    assert(restored.validate());

    std::cout << "test_serialization_roundtrip_with_data passed.\n";
}

void test_serialization_empty_buffer_returns_default() {
    ProgressionSnapshot restored = ProgressionSnapshot::deserialize(nullptr, 0);
    assert(restored.version == ProgressionSnapshot::kCurrentVersion);
    assert(restored.validate());
    std::cout << "test_serialization_empty_buffer_returns_default passed.\n";
}

void test_serialization_truncated_buffer_returns_repaired() {
    ProgressionSnapshot snap = ProgressionSnapshot::create_default();
    snap.xp = 5000;
    snap.currency = 300;
    snap.weapons.push_back({5, 1, true, 500});

    std::vector<std::uint8_t> data = snap.serialize();

    // Truncate to just the version header
    std::vector<std::uint8_t> truncated(data.begin(), data.begin() + 6);

    // Should not crash — returns repaired snapshot
    ProgressionSnapshot restored = ProgressionSnapshot::deserialize(truncated.data(), truncated.size());
    assert(restored.validate());

    std::cout << "test_serialization_truncated_buffer_returns_repaired passed.\n";
}

void test_serialization_corrupt_version() {
    ProgressionSnapshot snap = ProgressionSnapshot::create_default();
    snap.xp = 5000;

    std::vector<std::uint8_t> data = snap.serialize();
    // Corrupt the version field
    data[0] = 0xFF;
    data[1] = 0xFF;
    data[2] = 0xFF;
    data[3] = 0xFF;

    ProgressionSnapshot restored = ProgressionSnapshot::deserialize(data.data(), data.size());
    assert(restored.version == ProgressionSnapshot::kCurrentVersion);
    assert(restored.validate());
    // Data is lost due to unknown version being reset
    assert(restored.xp == 0);

    std::cout << "test_serialization_corrupt_version passed.\n";
}

// ===================================================================
// Integration tests
// ===================================================================

void test_integration_no_silent_discard() {
    // Verify that the processor never silently discards a reward.
    // Every call to process_grant must return a result that indicates
    // what happened with no ambiguity.

    Inventory inv{};
    PickupProcessor proc{};

    // Set up: make all resource types full to get explicit AtCap results
    inv.set_primary_ammo(300);
    inv.set_secondary_ammo(200);
    inv.set_heavy_ammo(50);
    inv.set_special_ammo(100);
    inv.set_current_health(100.0F);
    inv.set_current_shield(100.0F);
    inv.set_grenade_count(2);
    inv.set_ultimate_charge(1.0F);

    // Test each type returns a non-Granted result explicitly
    {
        RewardGrant g{{1, 1, 1}, PickupType::PrimaryAmmo, 30};
        assert(proc.process_grant(inv, g) == GrantResult::AtCap);
    }
    {
        RewardGrant g{{1, 1, 2}, PickupType::HealthPack, 20};
        assert(proc.process_grant(inv, g) == GrantResult::AtCap);
    }
    {
        RewardGrant g{{1, 1, 3}, PickupType::ShieldCharge, 30};
        assert(proc.process_grant(inv, g) == GrantResult::AtCap);
    }
    {
        RewardGrant g{{1, 1, 4}, PickupType::GrenadeCharge, 1};
        assert(proc.process_grant(inv, g) == GrantResult::AtCap);
    }
    {
        RewardGrant g{{1, 1, 5}, PickupType::UltimateEnergy, 0.5F};
        assert(proc.process_grant(inv, g) == GrantResult::AtCap);
    }

    std::cout << "test_integration_no_silent_discard passed.\n";
}

void test_integration_complete_pickup_cycle() {
    // Simulate a full game session with multiple pickups
    Inventory inv{};
    PickupProcessor proc{};

    ae::u32 seq = 0;

    // Player starts with some initial state
    inv.set_current_health(50.0F);   // damaged
    inv.set_current_shield(20.0F);   // shield depleted
    inv.set_primary_ammo(60);        // low on ammo
    inv.set_grenade_count(0);        // out of grenades

    // Pick up health pack
    {
        RewardGrant g{{1, 1, ++seq}, PickupType::HealthPack, 30};
        assert(proc.process_grant(inv, g) == GrantResult::Granted);
        assert(inv.current_health() == 80.0F);
    }

    // Pick up shield charge
    {
        RewardGrant g{{1, 1, ++seq}, PickupType::ShieldCharge, 50};
        assert(proc.process_grant(inv, g) == GrantResult::Granted);
        assert(inv.current_shield() == 70.0F);
    }

    // Pick up primary ammo
    {
        RewardGrant g{{1, 1, ++seq}, PickupType::PrimaryAmmo, 120};
        assert(proc.process_grant(inv, g) == GrantResult::Granted);
        assert(inv.primary_ammo() == 180);
    }

    // Pick up grenade charge
    {
        RewardGrant g{{1, 1, ++seq}, PickupType::GrenadeCharge, 1};
        assert(proc.process_grant(inv, g) == GrantResult::Granted);
        assert(inv.grenade_count() == 1);
    }

    // Pick up ultimate energy
    {
        RewardGrant g{{1, 1, ++seq}, PickupType::UltimateEnergy, 0.5F};
        assert(proc.process_grant(inv, g) == GrantResult::Granted);
        assert(inv.ultimate_charge() >= 0.49F);
    }

    // Pick up XP
    {
        RewardGrant g{{1, 1, ++seq}, PickupType::XPOrb, 250};
        assert(proc.process_grant(inv, g) == GrantResult::Granted);
        assert(inv.xp() == 250);
    }

    // Pick up a weapon
    {
        RewardGrant g{{1, 1, ++seq}, PickupType::WeaponDrop, 1, 42};
        assert(proc.process_grant(inv, g) == GrantResult::Granted);
        assert(inv.has_weapon(42));
    }

    // Verify all grants are idempotent
    for (ae::u32 i = 1; i <= seq; ++i) {
        RewardGrant g{{1, 1, i}};
        assert(proc.has_processed(g.id));
    }

    std::cout << "test_integration_complete_pickup_cycle passed.\n";
}

void test_integration_inventory_manager_combined_with_processor() {
    // Test that Inventory and PickupProcessor work together correctly
    Inventory inv{};
    PickupProcessor proc{};

    // Fill weapon inventory to test overflow behavior
    for (ae::u32 i = 1; i <= InventoryLimits::kMaxWeaponSlots; ++i) {
        RewardGrant g{{1, 1, i}, PickupType::WeaponDrop, 1, i};
        assert(proc.process_grant(inv, g) == GrantResult::Granted);
    }
    assert(inv.weapons_full());

    // Attempt to add one more
    {
        RewardGrant g{{1, 1, InventoryLimits::kMaxWeaponSlots + 1},
                       PickupType::WeaponDrop, 1, 999};
        assert(proc.process_grant(inv, g) == GrantResult::InventoryFull);
    }

    // Remove a weapon from inventory directly (e.g., player dismantles it)
    assert(inv.remove_weapon(inv.weapons().front().instance_id));

    // Should be able to add a new weapon now
    {
        RewardGrant g{{1, 1, InventoryLimits::kMaxWeaponSlots + 2},
                       PickupType::WeaponDrop, 1, 1000};
        assert(proc.process_grant(inv, g) == GrantResult::Granted);
        assert(inv.has_weapon(1000));
    }

    std::cout << "test_integration_inventory_manager_combined_with_processor passed.\n";
}

void test_integration_snapshot_save_and_load() {
    // Simulate saving a full game state and restoring it
    Inventory inv{};
    PickupProcessor proc{};

    // Start damaged
    inv.set_current_health(50.0F);

    // Play through some pickups
    assert(proc.process_grant(inv, {{1, 1, 1}, PickupType::XPOrb, 1000}) == GrantResult::Granted);
    assert(proc.process_grant(inv, {{1, 1, 2}, PickupType::Currency, 500}) == GrantResult::Granted);
    assert(proc.process_grant(inv, {{1, 1, 3}, PickupType::HealthPack, 50}) == GrantResult::Granted);
    assert(proc.process_grant(inv, {{1, 1, 4}, PickupType::WeaponDrop, 1, 7}) == GrantResult::Granted);
    assert(proc.process_grant(inv, {{1, 1, 5}, PickupType::WeaponDrop, 1, 8}) == GrantResult::Granted);

    // Build snapshot from current state
    ProgressionSnapshot snap = ProgressionSnapshot::create_default();
    snap.xp = inv.xp();
    snap.currency = inv.currency();
    snap.max_health = inv.max_health();
    snap.max_shield = inv.max_shield();
    snap.weapons = inv.weapons();
    snap.processed_grants = proc.processed_grants();

    assert(snap.validate());

    // Serialize
    std::vector<std::uint8_t> data = snap.serialize();

    // Deserialize
    ProgressionSnapshot loaded = ProgressionSnapshot::deserialize(data.data(), data.size());
    assert(loaded.validate());
    assert(loaded.xp == 1000);
    assert(loaded.currency == 500);
    assert(loaded.weapons.size() == 2);
    assert(loaded.processed_grants.size() == 5);

    // Reconstruct state from loaded snapshot
    Inventory loaded_inv{};
    loaded_inv.set_xp(loaded.xp);
    loaded_inv.set_currency(loaded.currency);
    for (const auto& w : loaded.weapons) {
        loaded_inv.add_weapon(w);
    }

    assert(loaded_inv.xp() == 1000);
    assert(loaded_inv.currency() == 500);
    assert(loaded_inv.has_weapon(7));
    assert(loaded_inv.has_weapon(8));
    assert(!loaded_inv.has_weapon(999));

    std::cout << "test_integration_snapshot_save_and_load passed.\n";
}

void test_integration_duplicate_grant_different_sessions() {
    // Two different sessions should be able to grant the same thing
    Inventory inv{};
    PickupProcessor proc{};

    // Session 1: grant XP
    {
        RewardGrant g{{10, 1, 1}, PickupType::XPOrb, 100};
        assert(proc.process_grant(inv, g) == GrantResult::Granted);
    }

    // Prune old session grants (simulate new session)
    proc.prune_grants_before_session(2);

    // Session 2: same source_id, same sequence, but different session
    {
        RewardGrant g{{10, 2, 1}, PickupType::XPOrb, 100};
        assert(proc.process_grant(inv, g) == GrantResult::Granted);
    }

    assert(inv.xp() == 200);

    std::cout << "test_integration_duplicate_grant_different_sessions passed.\n";
}

}  // anonymous namespace

int main() {
    // GrantId
    test_grant_id_default();
    test_grant_id_to_string_roundtrip();
    test_grant_id_from_string_malformed();
    test_grant_id_equality();
    test_grant_id_from_string_empty();

    // RewardGrant
    test_reward_grant_valid();
    test_reward_grant_invalid_type();
    test_reward_grant_zero_quantity();
    test_reward_grant_negative_quantity();

    // Inventory
    test_inventory_default_state();
    test_inventory_reset();
    test_inventory_weapon_management();
    test_inventory_armor_management();
    test_inventory_perk_management();
    test_inventory_resource_caps();

    // PickupProcessor — ammo
    test_processor_grant_ammo();
    test_processor_ammo_at_cap();
    test_processor_ammo_partial_cap();
    test_processor_all_ammo_types();

    // PickupProcessor — health & shield
    test_processor_grant_health();
    test_processor_health_full();
    test_processor_health_partial();
    test_processor_grant_shield();
    test_processor_shield_full();

    // PickupProcessor — items
    test_processor_grant_weapon_drop();
    test_processor_weapon_drop_duplicate();
    test_processor_weapon_drop_inventory_full();
    test_processor_grant_armor_drop();
    test_processor_grant_perk_drop();

    // PickupProcessor — abilities
    test_processor_grenade_charge();
    test_processor_grenade_at_cap();
    test_processor_ultimate_energy();
    test_processor_ultimate_at_cap();

    // PickupProcessor — progression resources
    test_processor_xp();
    test_processor_currency();
    test_processor_currency_overflow_saturation();

    // Idempotency
    test_processor_idempotency_duplicate_grant();
    test_processor_idempotency_different_grants();
    test_processor_reset_idempotency();
    test_processor_prune_grants_before_session();

    // Invalid grants
    test_processor_invalid_grant();
    test_processor_zero_quantity_grant();
    test_processor_negative_quantity_grant();
    test_processor_weapon_drop_zero_def_id();

    // ProgressionSnapshot
    test_snapshot_create_default();
    test_snapshot_validate_valid();
    test_snapshot_validate_invalid_version();
    test_snapshot_validate_oversized_collections();
    test_snapshot_validate_zero_def_id();
    test_snapshot_migrate_current();
    test_snapshot_migrate_unknown_version();
    test_snapshot_repair_negative_values();
    test_snapshot_repair_oversized_collections();
    test_snapshot_repair_zero_def_ids();
    test_snapshot_repair_unknown_version();
    test_snapshot_repair_excessive_values();

    // Serialization
    test_serialization_roundtrip_default();
    test_serialization_roundtrip_with_data();
    test_serialization_empty_buffer_returns_default();
    test_serialization_truncated_buffer_returns_repaired();
    test_serialization_corrupt_version();

    // Integration
    test_integration_no_silent_discard();
    test_integration_complete_pickup_cycle();
    test_integration_inventory_manager_combined_with_processor();
    test_integration_snapshot_save_and_load();
    test_integration_duplicate_grant_different_sessions();

    std::cout << "\nAll pickup model tests passed.\n";
    return 0;
}
