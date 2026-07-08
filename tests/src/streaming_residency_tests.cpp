// Streaming residency tests.
//
// Validates ResidencyManager: initialisation, region tracking, load/unload
// transitions when the player moves between regions, and boundary behaviour
// at grid edges.

#include "ae/core/residency_manager.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool near_eq(float a, float b, float eps = 0.0001F) {
    return std::fabs(a - b) < eps;
}

int fail(const std::string& msg) {
    std::cerr << "streaming_residency_tests failed: " << msg << '\n';
    return 1;
}

// ---------------------------------------------------------------------------
// Construction & initialisation
// ---------------------------------------------------------------------------

int test_init_default() {
    ae::core::ResidencyManager rm;
    if (!rm.empty()) return fail("default manager should be empty");
    if (rm.cols() != 1) return fail("default cols should be 1");
    if (rm.rows() != 1) return fail("default rows should be 1");
    if (rm.region_count() != 0) return fail("default region count should be 0");
    if (rm.resident_count() != 0) return fail("default resident count should be 0");
    if (rm.load_radius() != 1) return fail("default load radius should be 1");
    return 0;
}

int test_init_custom() {
    ae::core::ResidencyManager rm;
    rm.init(10, 8, 2);
    if (rm.empty()) return fail("initialised manager should not be empty");
    if (rm.cols() != 10) return fail("expected 10 cols");
    if (rm.rows() != 8) return fail("expected 8 rows");
    if (rm.load_radius() != 2) return fail("expected load radius 2");
    if (rm.region_count() != 80) return fail("expected 80 regions for 10x8 grid");
    if (rm.resident_count() != 0) return fail("initial resident count should be 0");
    return 0;
}

int test_init_clamps_zeros() {
    ae::core::ResidencyManager rm;
    rm.init(0, 0, -1);
    if (rm.cols() != 1) return fail("clamped cols should be 1");
    if (rm.rows() != 1) return fail("clamped rows should be 1");
    if (rm.load_radius() != 1) return fail("clamped load radius should be 1");
    if (rm.region_count() != 1) return fail("expected 1 region for clamped 1x1");
    return 0;
}

// ---------------------------------------------------------------------------
// Residency tracking after update
// ---------------------------------------------------------------------------

int test_player_starts_in_origin_region() {
    ae::core::ResidencyManager rm;
    rm.init(5, 5, 0);
    // Player at world origin — grid origin also at (0,0), cell_size=10.
    rm.update(5.0F, 5.0F, 10.0F, 0.0F, 0.0F);

    // With load_radius=0, only the player's region should be resident.
    const auto pr = rm.player_region();
    if (pr.cx != 0 || pr.cy != 0) return fail("player should be in region (0,0)");

    if (rm.resident_count() != 1) return fail("with radius 0, only 1 region should be resident");
    if (!rm.is_resident(0, 0)) return fail("region (0,0) should be resident");
    if (rm.is_resident(1, 0)) return fail("region (1,0) should NOT be resident");
    return 0;
}

int test_load_radius_square() {
    ae::core::ResidencyManager rm;
    rm.init(10, 10, 1);
    rm.update(25.0F, 25.0F, 10.0F, 0.0F, 0.0F);

    // Player is in region (2,2).  With Chebyshev radius 1, a 3x3 block of
    // regions from (1,1) to (3,3) should be resident = 9 regions.
    const auto pr = rm.player_region();
    if (pr.cx != 2 || pr.cy != 2) return fail("player should be in region (2,2)");

    if (rm.resident_count() != 9) return fail("Chebyshev radius 1 -> 3x3 = 9 regions");

    // Centre ring should be resident.
    for (int cy = 1; cy <= 3; ++cy) {
        for (int cx = 1; cx <= 3; ++cx) {
            if (!rm.is_resident(cx, cy)) {
                return fail("centre 3x3 block should all be resident");
            }
        }
    }
    // Outer ring should NOT be resident.
    if (rm.is_resident(0, 2)) return fail("outer (0,2) should NOT be resident");
    if (rm.is_resident(4, 2)) return fail("outer (4,2) should NOT be resident");
    return 0;
}

int test_large_load_radius() {
    ae::core::ResidencyManager rm;
    rm.init(10, 10, 3);
    rm.update(50.0F, 50.0F, 10.0F, 0.0F, 0.0F);

    // Player in (5,5).  Chebyshev radius 3 -> 7x7 = 49 regions centred on (5,5).
    // But the grid is only 10x10, so edges are clamped.
    if (rm.resident_count() != 49) return fail("radius 3 on 10x10 -> 7x7 = 49 in interior");
    return 0;
}

// ---------------------------------------------------------------------------
// Transitions: load on first update, unload on move
// ---------------------------------------------------------------------------

int test_initial_update_produces_load_transitions() {
    ae::core::ResidencyManager rm;
    rm.init(5, 5, 1);

    auto pending = rm.consume_pending();
    if (!pending.empty()) return fail("no update yet, pending should be empty");

    rm.update(0.0F, 0.0F, 10.0F, 0.0F, 0.0F);
    pending = rm.consume_pending();

    // Player in (0,0) with radius 1 on a 5x5 grid -> 2x2 = 4 regions.
    // After consuming, pending should be empty.
    if (pending.empty()) return fail("first update should produce load transitions");
    if (pending.size() != 4) {
        return fail("expected 4 load transitions for radius 1 at corner");
    }
    for (const auto& t : pending) {
        if (!t.load) return fail("all transitions should be loads initially");
    }
    // Pending should now be cleared
    if (rm.pending_count() != 0) return fail("pending should be cleared after consume");
    return 0;
}

int test_move_triggers_unload_and_load() {
    ae::core::ResidencyManager rm;
    rm.init(10, 10, 1);

    // Position 1: player in region (0,0).
    rm.update(5.0F, 5.0F, 10.0F, 0.0F, 0.0F);
    auto pending = rm.consume_pending();
    if (pending.size() != 4) return fail("expected 4 initial loads at (0,0) with radius 1");

    // Position 2: move far right to region (9,9).
    rm.update(95.0F, 95.0F, 10.0F, 0.0F, 0.0F);
    pending = rm.consume_pending();

    if (pending.empty()) return fail("move should produce transitions");

    // At least some old regions should be unloads and some new regions loads.
    bool has_unload = false;
    bool has_load = false;
    for (const auto& t : pending) {
        if (t.load) has_load = true;
        else has_unload = true;
    }
    if (!has_unload) return fail("expected at least one unload after move");
    if (!has_load) return fail("expected at least one load after move");

    // After update, only the (9,9) corner neighbourhood should be resident.
    if (rm.is_resident(0, 0)) return fail("region (0,0) should have been unloaded");
    if (!rm.is_resident(9, 9)) return fail("region (9,9) should be resident");
    if (!rm.is_resident(8, 9)) return fail("region (8,9) should be resident");
    if (!rm.is_resident(9, 8)) return fail("region (9,8) should be resident");
    return 0;
}

int test_move_crosses_one_region_boundary() {
    ae::core::ResidencyManager rm;
    rm.init(10, 10, 1);

    // Start in region (2,2).
    rm.update(25.0F, 25.0F, 10.0F, 0.0F, 0.0F);
    (void)rm.consume_pending(); // discard initial loads

    // Move one cell to the right: region (3,2).
    rm.update(35.0F, 25.0F, 10.0F, 0.0F, 0.0F);
    auto pending = rm.consume_pending();

    if (pending.empty()) return fail("crossing a region boundary should produce transitions");

    // Expected: left column (cx=0) of the old 3x3 unloads, right column
    // (cx=4) of the new 3x3 loads.  That's 3 unloads and 3 loads = 6 transitions.
    if (pending.size() != 6) {
        return fail("expected 6 transitions when moving one region with radius 1 (3 unload + 3 load)");
    }

    int load_count = 0;
    int unload_count = 0;
    for (const auto& t : pending) {
        if (t.load) ++load_count;
        else ++unload_count;
    }
    if (unload_count != 3) return fail("expected 3 unloads when stepping right one region");
    if (load_count != 3) return fail("expected 3 loads when stepping right one region");

    // Verify resident state after transition.
    if (rm.is_resident(1, 1)) return fail("old far-left column should be unloaded");
    if (!rm.is_resident(3, 2)) return fail("current region should be resident");
    if (!rm.is_resident(4, 2)) return fail("new right column should be resident");
    return 0;
}

// ---------------------------------------------------------------------------
// Edge / boundary behaviour
// ---------------------------------------------------------------------------

int test_corner_edge_cases() {
    ae::core::ResidencyManager rm;
    rm.init(3, 3, 2);

    // Player at (0,0) corner with radius 2 on a 3x3 grid.
    // Entire grid (3x3 = 9 regions) should be resident.
    rm.update(5.0F, 5.0F, 10.0F, 0.0F, 0.0F);
    if (rm.resident_count() != 9) return fail("radius 2 on 3x3 should load all regions");
    for (int cy = 0; cy < 3; ++cy) {
        for (int cx = 0; cx < 3; ++cx) {
            if (!rm.is_resident(cx, cy)) return fail("all regions should be resident on 3x3");
        }
    }
    return 0;
}

int test_out_of_bounds_query() {
    ae::core::ResidencyManager rm;
    rm.init(5, 5, 1);
    rm.update(0.0F, 0.0F, 10.0F, 0.0F, 0.0F);

    // Query regions outside the grid should return false, not crash.
    if (rm.is_resident(-1, 0)) return fail("negative cx should not be resident");
    if (rm.is_resident(0, -1)) return fail("negative cy should not be resident");
    if (rm.is_resident(5, 0)) return fail("out-of-range cx should not be resident");
    if (rm.is_resident(0, 5)) return fail("out-of-range cy should not be resident");
    return 0;
}

int test_player_outside_grid_clamped() {
    ae::core::ResidencyManager rm;
    rm.init(5, 5, 1);

    // Player well outside the grid (negative coordinates, far beyond).
    rm.update(-100.0F, -100.0F, 10.0F, 0.0F, 0.0F);

    // Player region should be clamped to (0,0).
    const auto pr = rm.player_region();
    if (pr.cx != 0 || pr.cy != 0) return fail("out-of-bounds player should be clamped to (0,0)");
    if (rm.resident_count() == 0) return fail("should still have resident regions when clamped");
    return 0;
}

// ---------------------------------------------------------------------------
// Load radius 0: only the player's immediate region
// ---------------------------------------------------------------------------

int test_radius_zero() {
    ae::core::ResidencyManager rm;
    rm.init(5, 5, 0);

    rm.update(25.0F, 25.0F, 10.0F, 0.0F, 0.0F);
    if (rm.resident_count() != 1) return fail("radius 0 should have exactly 1 resident region");
    if (!rm.is_resident(2, 2)) return fail("radius 0: player's own region should be resident");
    if (rm.is_resident(1, 2)) return fail("radius 0: neighbouring region should not be resident");
    return 0;
}

// ---------------------------------------------------------------------------
// Multiple frames without movement: no transitions
// ---------------------------------------------------------------------------

int test_no_movement_no_transitions() {
    ae::core::ResidencyManager rm;
    rm.init(5, 5, 1);
    rm.update(25.0F, 25.0F, 10.0F, 0.0F, 0.0F);
    (void)rm.consume_pending(); // discard initial loads

    // Second update in the same position.
    rm.update(25.0F, 25.0F, 10.0F, 0.0F, 0.0F);
    auto pending = rm.consume_pending();
    if (!pending.empty()) return fail("no transitions should be produced when player hasn't moved");
    if (rm.resident_count() != 9) return fail("residency should remain unchanged");
    return 0;
}

}  // namespace

int main() {
    // Construction & initialisation
    if (int rc = test_init_default()) return rc;
    if (int rc = test_init_custom()) return rc;
    if (int rc = test_init_clamps_zeros()) return rc;

    // Residency tracking
    if (int rc = test_player_starts_in_origin_region()) return rc;
    if (int rc = test_load_radius_square()) return rc;
    if (int rc = test_large_load_radius()) return rc;

    // Transitions
    if (int rc = test_initial_update_produces_load_transitions()) return rc;
    if (int rc = test_move_triggers_unload_and_load()) return rc;
    if (int rc = test_move_crosses_one_region_boundary()) return rc;

    // Edge cases
    if (int rc = test_corner_edge_cases()) return rc;
    if (int rc = test_out_of_bounds_query()) return rc;
    if (int rc = test_player_outside_grid_clamped()) return rc;

    // Radius zero
    if (int rc = test_radius_zero()) return rc;

    // No movement = no transitions
    if (int rc = test_no_movement_no_transitions()) return rc;

    std::cout << "streaming_residency_tests: all 12 tests passed\n";
    (void)near_eq;  // suppress unused warning
    return 0;
}
