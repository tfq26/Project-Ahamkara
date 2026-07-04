#include "ahamkara/game/weapon_loader.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace ahamkara::game;

void test_stat_key_mapping() {
    assert(stat_key_from_string("damage") == StatKey::damage);
    assert(stat_key_from_string("range") == StatKey::range);
    assert(stat_key_from_string("stability") == StatKey::stability);
    assert(stat_key_from_string("magazine_size_mult") == StatKey::magazine_size_mult);
    assert(stat_key_from_string("damage_mult") == StatKey::damage_mult);
    assert(stat_key_from_string("nonexistent") == StatKey::_count);
    std::cout << "test_stat_key_mapping passed.\n";
}

void test_load_and_build_instance() {
    WeaponDatabase db;
    bool ok = db.load_json("tools/blender/weapons/archetypes.json",
                           "tools/blender/weapons/perks.json");
    assert(ok);

    const auto* arch = db.find_archetype("ar15");
    assert(arch != nullptr);
    assert(arch->name == "AR-15");
    assert(arch->slot == "primary");
    assert(arch->mesh == "viewmodel_ar15");
    assert(arch->fire_mode == "automatic");
    assert(arch->perk_slots.size() == 4);
    assert(arch->base_stats[static_cast<int>(StatKey::damage)] == 20.0F);

    const auto* perk = db.find_perk("rampage");
    assert(perk != nullptr);
    assert(perk->slot == "trait1");

    auto inst = db.build_instance("ar15", {"extended_mag", "rampage"});
    assert(inst.archetype_id == "ar15");
    assert(inst.perk_ids.size() == 2);

    // extended_mag multiplies magazine_size by 1.5: 50 * 1.5 = 75
    float mag = inst.stats[static_cast<int>(StatKey::magazine_size)];
    assert(mag >= 70.0F && mag <= 80.0F);

    std::cout << "test_load_and_build_instance passed.\n";
}

void test_shotgun_perks() {
    WeaponDatabase db;
    bool ok = db.load_json("tools/blender/weapons/archetypes.json",
                           "tools/blender/weapons/perks.json");
    assert(ok);

    auto inst = db.build_instance("shotgun", {"full_bore", "tactical_mag", "snapshot_sights"});
    assert(inst.perk_ids.size() == 3);

    // full_bore: range +25, stability -10, handling -10
    float range = inst.stats[static_cast<int>(StatKey::range)];
    assert(range > 35.0F); // base 20 + 25 full_bore

    float stability = inst.stats[static_cast<int>(StatKey::stability)];
    assert(stability < 35.0F); // base 40 - 10 full_bore

    std::cout << "test_shotgun_perks passed.\n";
}

void test_unknown_archetype() {
    WeaponDatabase db;
    db.load_json("tools/blender/weapons/archetypes.json",
                 "tools/blender/weapons/perks.json");

    const auto* arch = db.find_archetype("sword");
    assert(arch == nullptr);

    const auto* perk = db.find_perk("fire_breath");
    assert(perk == nullptr);

    std::cout << "test_unknown_archetype passed.\n";
}

void test_missing_file() {
    WeaponDatabase db;
    bool ok = db.load_json("nonexistent.json", "also_nonexistent.json");
    assert(!ok);
    std::cout << "test_missing_file passed.\n";
}

}  // namespace

int main() {
    test_stat_key_mapping();
    test_load_and_build_instance();
    test_shotgun_perks();
    test_unknown_archetype();
    test_missing_file();

    std::cout << "\nAll weapon loader tests passed.\n";
    return 0;
}
