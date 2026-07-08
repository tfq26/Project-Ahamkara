// Destination metadata tests.
//
// Validates DestinationMetadata types: struct layout, static definitions,
// field access, and null-pointer safety in WorldDefinition.

#include "ahamkara/game/destination_metadata.h"
#include "ahamkara/game/worlds/world_definition.h"

#include <cstddef>
#include <iostream>
#include <string>
#include <type_traits>

namespace {

int fail(const std::string& msg) {
    std::cerr << "destination_metadata_tests failed: " << msg << '\n';
    return 1;
}

// ---------------------------------------------------------------------------
// Compile-time struct properties
// ---------------------------------------------------------------------------

int test_destination_metadata_is_standard_layout() {
    if (!std::is_standard_layout<ahamkara::game::DestinationMetadata>::value)
        return fail("DestinationMetadata should be standard layout");
    if (!std::is_standard_layout<ahamkara::game::RegionDescriptor>::value)
        return fail("RegionDescriptor should be standard layout");
    if (!std::is_standard_layout<ahamkara::game::LandingZoneDefinition>::value)
        return fail("LandingZoneDefinition should be standard layout");
    if (!std::is_standard_layout<ahamkara::game::AmbientPopulationSpawn>::value)
        return fail("AmbientPopulationSpawn should be standard layout");
    if (!std::is_standard_layout<ahamkara::game::RegionBounds>::value)
        return fail("RegionBounds should be standard layout");
    return 0;
}

int test_region_type_count_sentinel() {
    // Count should have a reasonable number of types.
    if (static_cast<int>(ahamkara::game::RegionType::Count) != 7)
        return fail("RegionType::Count should be 7 (7 real types)");
    return 0;
}

// ---------------------------------------------------------------------------
// Default values
// ---------------------------------------------------------------------------

int test_destination_metadata_defaults() {
    ahamkara::game::DestinationMetadata dm;
    if (dm.id != std::string("")) return fail("default id should be empty");
    if (dm.display_name != std::string("")) return fail("default display_name should be empty");
    if (dm.description != std::string("")) return fail("default description should be empty");
    if (dm.grid_cols != 1) return fail("default grid_cols should be 1");
    if (dm.grid_rows != 1) return fail("default grid_rows should be 1");
    if (dm.cell_size != 100.0F) return fail("default cell_size should be 100.0");
    if (dm.regions != nullptr) return fail("default regions should be null");
    if (dm.region_count != 0) return fail("default region_count should be 0");
    if (dm.landing_zones != nullptr) return fail("default landing_zones should be null");
    if (dm.landing_zone_count != 0) return fail("default landing_zone_count should be 0");
    if (dm.default_population != nullptr) return fail("default default_population should be null");
    return 0;
}

int test_region_descriptor_defaults() {
    ahamkara::game::RegionDescriptor rd;
    if (rd.id != std::string("")) return fail("default region id should be empty");
    if (rd.type != ahamkara::game::RegionType::Exploration)
        return fail("default region type should be Exploration");
    if (!rd.is_combat_zone) return fail("default region should be a combat zone");
    if (!rd.is_landing_allowed) return fail("default region should allow landing");
    if (rd.recommended_min_level != 1) return fail("default min level should be 1");
    if (rd.recommended_max_level != 10) return fail("default max level should be 10");
    if (rd.ambient_population != nullptr) return fail("default ambient_population should be null");
    if (rd.landing_zones != nullptr) return fail("default landing_zones should be null");
    return 0;
}

int test_landing_zone_defaults() {
    ahamkara::game::LandingZoneDefinition lz;
    if (lz.id != std::string("")) return fail("default landing zone id should be empty");
    if (lz.is_primary) return fail("default is_primary should be false");
    if (lz.requires_unlock) return fail("default requires_unlock should be false");
    if (lz.yaw != 0.0F) return fail("default yaw should be 0");
    return 0;
}

int test_ambient_population_defaults() {
    ahamkara::game::AmbientPopulationSpawn ap;
    if (ap.npc_id != std::string("")) return fail("default npc_id should be empty");
    if (ap.min_count != 0) return fail("default min_count should be 0");
    if (ap.max_count != 3) return fail("default max_count should be 3");
    if (ap.respawn_time_seconds != 30.0F) return fail("default respawn_time should be 30.0");
    if (ap.spawn_radius != 10.0F) return fail("default spawn_radius should be 10.0");
    return 0;
}

int test_region_bounds_defaults() {
    ahamkara::game::RegionBounds rb;
    if (rb.min_x != 0.0F || rb.min_z != 0.0F) return fail("default region bounds min should be 0");
    if (rb.max_x != 0.0F || rb.max_z != 0.0F) return fail("default region bounds max should be 0");
    if (rb.min_y != -100.0F) return fail("default region bounds min_y should be -100");
    if (rb.max_y != 100.0F) return fail("default region bounds max_y should be 100");
    return 0;
}

// ---------------------------------------------------------------------------
// Static destination definition (integration-style test)
// ---------------------------------------------------------------------------

// Sample population: two creature types.
constexpr ahamkara::game::AmbientPopulationSpawn kForestPopulation[] = {
    {"npc_dreg", 2, 5, 30.0F, 15.0F, "group_forest_common"},
    {"npc_woodland_fauna", 1, 3, 20.0F, 20.0F, "group_forest_fauna"},
};

// Sample landing zones.
constexpr ahamkara::game::LandingZoneDefinition kForestLandingZones[] = {
    {"landing_crossroads", "Crossroads Landing", -50.0F, 0.0F, 0.0F, 0.0F, true, false, ""},
    {"landing_temple", "Temple Approach", 100.0F, 0.0F, 50.0F, 180.0F, false, true, "complete_quest_temple_gate"},
};

// Sample regions.
constexpr ahamkara::game::RegionDescriptor kForestRegions[] = {
    {
        "region_crossroads",
        "Crossroads",
        ahamkara::game::RegionType::Hub,
        {-60.0F, -30.0F, 60.0F, 30.0F, -100.0F, 100.0F},
        "The central hub of the Ancient Forest",
        1, 10,
        false,   // is_combat_zone = false (hub)
        true,    // is_landing_allowed
        kForestPopulation, 2,  // uses default forest population
        kForestLandingZones, 1  // only the crossroads landing zone
    },
    {
        "region_temple_entrance",
        "Temple Entrance",
        ahamkara::game::RegionType::Combat,
        {60.0F, -20.0F, 140.0F, 60.0F, -100.0F, 100.0F},
        "The approach to the ancient temple",
        5, 15,
        true,    // is_combat_zone
        true,    // is_landing_allowed
        nullptr, 0,  // no explicit population (inherits default)
        kForestLandingZones + 1, 1  // temple landing zone
    },
    {
        "region_forest_exploration",
        "Forest Outer Ring",
        ahamkara::game::RegionType::Exploration,
        {-140.0F, -100.0F, 140.0F, 100.0F, -100.0F, 100.0F},
        "The wild outer ring of the Ancient Forest",
        1, 8,
        true,    // is_combat_zone
        false,   // is_landing_allowed (must enter from hub)
        nullptr, 0,
        nullptr, 0
    },
};

// Sample default population (used when region has no explicit population).
constexpr ahamkara::game::AmbientPopulationSpawn kDefaultPopulation[] = {
    {"npc_dreg", 1, 3, 30.0F, 15.0F, "group_default"},
};

// Complete destination definition.
constexpr ahamkara::game::DestinationMetadata kAncientForest = {
    "destination_ancient_forest",
    "Ancient Forest",
    "A dense woodland teeming with wildlife and ancient ruins",
    -150.0F, -110.0F, 150.0F, 110.0F,  // world bounds
    3, 3, 100.0F,  // grid: 3x3 with 100m cells
    kForestRegions, 3,                           // regions
    kForestLandingZones, 2,                      // landing zones
    kDefaultPopulation, 1                        // default population
};

int test_static_destination_fields() {
    if (kAncientForest.id != std::string("destination_ancient_forest"))
        return fail("destination id mismatch");
    if (kAncientForest.display_name != std::string("Ancient Forest"))
        return fail("destination display_name mismatch");
    if (kAncientForest.world_min_x != -150.0F)
        return fail("destination world_min_x mismatch");
    if (kAncientForest.world_max_x != 150.0F)
        return fail("destination world_max_x mismatch");
    if (kAncientForest.grid_cols != 3)
        return fail("destination grid_cols mismatch");
    if (kAncientForest.grid_rows != 3)
        return fail("destination grid_rows mismatch");
    if (kAncientForest.cell_size != 100.0F)
        return fail("destination cell_size mismatch");
    if (kAncientForest.region_count != 3)
        return fail("destination region_count mismatch");
    if (kAncientForest.landing_zone_count != 2)
        return fail("destination landing_zone_count mismatch");
    if (kAncientForest.default_population_count != 1)
        return fail("destination default_population_count mismatch");
    return 0;
}

int test_static_region_fields() {
    // Region 0: Crossroads (Hub)
    {
        const auto& r = kAncientForest.regions[0];
        if (r.id != std::string("region_crossroads"))
            return fail("region 0 id mismatch");
        if (r.type != ahamkara::game::RegionType::Hub)
            return fail("region 0 type should be Hub");
        if (r.is_combat_zone)
            return fail("region 0 (Hub) should not be a combat zone");
        if (!r.is_landing_allowed)
            return fail("region 0 should allow landing");
        if (r.ambient_population_count != 2)
            return fail("region 0 should have 2 population entries");
        if (r.landing_zone_count != 1)
            return fail("region 0 should have 1 landing zone");
        if (std::string(r.ambient_population[0].npc_id) != "npc_dreg")
            return fail("region 0 first population npc_id mismatch");
    }

    // Region 1: Temple Entrance (Combat)
    {
        const auto& r = kAncientForest.regions[1];
        if (r.id != std::string("region_temple_entrance"))
            return fail("region 1 id mismatch");
        if (r.type != ahamkara::game::RegionType::Combat)
            return fail("region 1 type should be Combat");
        if (!r.is_combat_zone)
            return fail("region 1 should be a combat zone");
        if (r.recommended_min_level != 5)
            return fail("region 1 recommended_min_level mismatch");
        if (r.recommended_max_level != 15)
            return fail("region 1 recommended_max_level mismatch");
        if (r.ambient_population != nullptr)
            return fail("region 1 should use null population (inherit default)");
        if (r.landing_zone_count != 1)
            return fail("region 1 should have 1 landing zone");
    }

    // Region 2: Forest Outer Ring (Exploration)
    {
        const auto& r = kAncientForest.regions[2];
        if (r.id != std::string("region_forest_exploration"))
            return fail("region 2 id mismatch");
        if (r.type != ahamkara::game::RegionType::Exploration)
            return fail("region 2 type should be Exploration");
        if (r.is_landing_allowed)
            return fail("region 2 should not allow landing");
        if (r.bounds.min_x != -140.0F)
            return fail("region 2 bounds.min_x mismatch");
        if (r.bounds.max_x != 140.0F)
            return fail("region 2 bounds.max_x mismatch");
    }

    return 0;
}

int test_static_landing_zone_fields() {
    const auto& lz0 = kAncientForest.landing_zones[0];
    if (lz0.id != std::string("landing_crossroads"))
        return fail("landing zone 0 id mismatch");
    if (!lz0.is_primary)
        return fail("landing zone 0 should be primary");
    if (lz0.requires_unlock)
        return fail("landing zone 0 should not require unlock");
    if (lz0.pos_x != -50.0F)
        return fail("landing zone 0 pos_x mismatch");

    const auto& lz1 = kAncientForest.landing_zones[1];
    if (lz1.id != std::string("landing_temple"))
        return fail("landing zone 1 id mismatch");
    if (lz1.is_primary)
        return fail("landing zone 1 should not be primary");
    if (!lz1.requires_unlock)
        return fail("landing zone 1 should require unlock");
    if (lz1.unlock_condition != std::string("complete_quest_temple_gate"))
        return fail("landing zone 1 unlock_condition mismatch");
    return 0;
}

int test_static_population_fields() {
    const auto& p0 = kForestPopulation[0];
    if (p0.npc_id != std::string("npc_dreg"))
        return fail("population 0 npc_id mismatch");
    if (p0.min_count != 2)
        return fail("population 0 min_count mismatch");
    if (p0.max_count != 5)
        return fail("population 0 max_count mismatch");
    if (p0.respawn_time_seconds != 30.0F)
        return fail("population 0 respawn_time mismatch");
    if (p0.spawn_radius != 15.0F)
        return fail("population 0 spawn_radius mismatch");
    if (p0.spawn_group != std::string("group_forest_common"))
        return fail("population 0 spawn_group mismatch");

    const auto& p1 = kForestPopulation[1];
    if (p1.npc_id != std::string("npc_woodland_fauna"))
        return fail("population 1 npc_id mismatch");
    if (p1.max_count != 3)
        return fail("population 1 max_count mismatch");
    if (p1.respawn_time_seconds != 20.0F)
        return fail("population 1 respawn_time mismatch");
    return 0;
}

// ---------------------------------------------------------------------------
// WorldDefinition with destination metadata
// ---------------------------------------------------------------------------

int test_world_definition_with_destination() {
    ahamkara::game::WorldDefinition wd;
    wd.id = "test_world";
    wd.destination = &kAncientForest;

    if (wd.id != std::string("test_world"))
        return fail("world id mismatch");
    if (wd.destination == nullptr)
        return fail("destination should not be null after assignment");
    if (wd.destination->display_name != std::string("Ancient Forest"))
        return fail("destination display_name via WorldDefinition mismatch");
    if (wd.destination->region_count != 3)
        return fail("destination region_count via WorldDefinition mismatch");

    // Default (null destination) should still work — existing level path.
    ahamkara::game::WorldDefinition wd_default;
    if (wd_default.destination != nullptr)
        return fail("default world definition destination should be null");
    return 0;
}

int test_is_valid_region_type() {
    using RT = ahamkara::game::RegionType;
    if (!ahamkara::game::is_valid_region_type(RT::Combat))
        return fail("Combat should be valid");
    if (!ahamkara::game::is_valid_region_type(RT::Social))
        return fail("Social should be valid");
    if (!ahamkara::game::is_valid_region_type(RT::Hub))
        return fail("Hub should be valid");
    if (!ahamkara::game::is_valid_region_type(static_cast<RT>(0)))
        return fail("RegionType(0) should be valid");
    if (ahamkara::game::is_valid_region_type(static_cast<RT>(99)))
        return fail("RegionType(99) should be invalid");
    if (ahamkara::game::is_valid_region_type(RT::Count))
        return fail("Count sentinel should not be valid");
    return 0;
}

}  // namespace

int main() {
    // Compile-time properties
    if (int rc = test_destination_metadata_is_standard_layout()) return rc;
    if (int rc = test_region_type_count_sentinel()) return rc;

    // Default values
    if (int rc = test_destination_metadata_defaults()) return rc;
    if (int rc = test_region_descriptor_defaults()) return rc;
    if (int rc = test_landing_zone_defaults()) return rc;
    if (int rc = test_ambient_population_defaults()) return rc;
    if (int rc = test_region_bounds_defaults()) return rc;

    // Static destination definition
    if (int rc = test_static_destination_fields()) return rc;
    if (int rc = test_static_region_fields()) return rc;
    if (int rc = test_static_landing_zone_fields()) return rc;
    if (int rc = test_static_population_fields()) return rc;

    // WorldDefinition integration
    if (int rc = test_world_definition_with_destination()) return rc;

    // Validation helpers
    if (int rc = test_is_valid_region_type()) return rc;

    std::cout << "destination_metadata_tests: all 13 tests passed\n";
    return 0;
}
