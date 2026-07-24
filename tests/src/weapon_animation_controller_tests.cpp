/// Unit tests for WeaponAnimationController reload animation sequence.
///
/// Tests the phase-driven reload animation: GrabMag → RemoveMag → InsertMag
/// → ReturnToGrip, verifying that the controller uses data-driven timing from
/// WeaponReloadData, computes correct IK offsets per phase, and stays synced
/// with the runtime reload state (snapshot.is_reloading).
///
/// These tests require ahamkara_client_lib (not available in headless builds).

#include "ahamkara/client/weapon_animation_controller.h"
#include "ahamkara/client/weapon_viewmodel_data.h"
#include "ahamkara/client/debug_scene_bridge.h"
#include "ahamkara/game/net_types.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <cstring>

namespace {

using namespace ahamkara::client;
using namespace ahamkara::game;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Create a minimal snapshot with the given weapon index.
ClientSimulationSnapshot make_snapshot(int weapon_index,
                                       float ammo_current = 25.0F,
                                       float ammo_max = 50.0F,
                                       int reserve = 150,
                                       bool is_reloading = false) {
    ClientSimulationSnapshot snap{};
    snap.weapon_index = weapon_index;
    snap.ammo_current = ammo_current;
    snap.ammo_max = ammo_max;
    snap.reserve_ammo = reserve;
    snap.is_reloading = is_reloading;

    // Zero out player state
    snap.player_state = {};
    snap.player_state.health = 100.0F;

    // Provide a default input
    return snap;
}

PlayerInputCommand make_input(bool reload_pressed = false) {
    PlayerInputCommand cmd{};
    cmd.reload_pressed = reload_pressed;
    return cmd;
}

/// Check that two floats are approximately equal.
bool approx(float a, float b, float epsilon = 0.001F) {
    return std::fabs(a - b) < epsilon;
}

/// Check that all three components of an IK offset are near zero.
bool ik_at_zero(const float* ik) {
    return ik != nullptr && approx(ik[0], 0.0F) && approx(ik[1], 0.0F) && approx(ik[2], 0.0F);
}

// ---------------------------------------------------------------------------
// Test: Weapon profiles have reload_data assigned
// ---------------------------------------------------------------------------

void test_profiles_have_reload_data() {
    WeaponAnimationController ctrl;
    ctrl.reset();

    // Trigger reload on each weapon and verify phases advance.
    // If reload_data were missing (default-constructed with all zeros),
    // the phase would stay stuck at Idle regardless of progress.

    for (int wi = 0; wi <= 2; ++wi) {
        ctrl.reset();

        auto snap = make_snapshot(wi, 25.0F, 50.0F, 150, false);
        auto input = make_input(true);

        // First tick establishes the weapon
        ctrl.tick(0.0F, snap, input);

        // Second tick with reload_pressed triggers the reload
        ctrl.tick(0.016F, snap, input);

        // Check that reload started (phase != Idle)
        const auto phase = ctrl.reload_phase();
        assert(phase != ReloadPhase::Idle);
    }

    std::cout << "test_profiles_have_reload_data passed.\n";
}

// ---------------------------------------------------------------------------
// Test: Reload starts on input.reload_pressed
// ---------------------------------------------------------------------------

void test_reload_starts_on_input() {
    WeaponAnimationController ctrl;
    ctrl.reset();

    auto snap = make_snapshot(0, 25.0F, 50.0F, 150, false);
    auto input = make_input(true); // reload_pressed

    // Tick to register weapon index
    ctrl.tick(0.0F, snap, input);
    assert(ctrl.reload_phase() == ReloadPhase::Idle);

    // Tick again with reload_pressed = true
    ctrl.tick(0.016F, snap, input);
    assert(ctrl.reload_phase() != ReloadPhase::Idle);
    assert(ctrl.reload_normalized() >= 0.0F);
    assert(ctrl.reload_normalized() < 0.1F); // just started

    std::cout << "test_reload_starts_on_input passed.\n";
}

// ---------------------------------------------------------------------------
// Test: Reload starts on snapshot.is_reloading (runtime sync)
// ---------------------------------------------------------------------------

void test_reload_starts_on_runtime_sync() {
    WeaponAnimationController ctrl;
    ctrl.reset();

    // Snapshot with is_reloading = true but no reload_pressed on input
    auto snap = make_snapshot(0, 25.0F, 50.0F, 150, /*is_reloading=*/true);
    auto input = make_input(false); // no reload_pressed

    // Tick to register weapon
    ctrl.tick(0.0F, snap, input);

    // Tick again — should start reload from is_reloading flag
    ctrl.tick(0.016F, snap, input);
    assert(ctrl.reload_phase() != ReloadPhase::Idle &&
           "Reload should start from snapshot.is_reloading even without input edge");

    std::cout << "test_reload_starts_on_runtime_sync passed.\n";
}

// ---------------------------------------------------------------------------
// Test: Phase transitions match expected ordering
// ---------------------------------------------------------------------------

void test_phase_transitions() {
    WeaponAnimationController ctrl;
    ctrl.reset();

    auto snap = make_snapshot(0, 25.0F, 50.0F, 150, false);
    auto input = make_input(true);

    // Start reload
    ctrl.tick(0.0F, snap, input);
    ctrl.tick(0.016F, snap, input);

    // Move through the reload by ticking large chunks.
    // AR-15 reload is 2.0s, so each 0.5s tick advances ~25%.
    const auto& rd = kWeaponReloadData[0]; // AR-15

    // Phase: GrabMag [0.00, 0.18]
    ctrl.tick(0.001F, snap, input); // tiny tick forward
    assert(ctrl.reload_phase() == ReloadPhase::GrabMag);

    // Tick to just past RemoveMag start
    // GrabMag: 0.00-0.18, RemoveMag: 0.18-0.45
    // 2.0s total * 0.19 = 0.38s — advance from (small) to 0.38s
    auto snap_reloading = make_snapshot(0, 25.0F, 50.0F, 150, true);

    // Advance timer to 25% progress (0.50s)
    ctrl.tick(0.48F, snap_reloading, input);
    // At ~25%, should be in RemoveMag (0.18-0.45)
    assert(ctrl.reload_phase() == ReloadPhase::RemoveMag);

    // Advance to 60% progress (1.20s) — should be in InsertMag (0.45-0.75)
    ctrl.tick(0.70F, snap_reloading, input);
    assert(ctrl.reload_phase() == ReloadPhase::InsertMag);

    // Advance to 90% progress (1.80s) — should be in ReturnToGrip (0.75-1.00)
    ctrl.tick(0.60F, snap_reloading, input);
    assert(ctrl.reload_phase() == ReloadPhase::ReturnToGrip);

    // Advance to completion
    ctrl.tick(0.5F, snap_reloading, input);
    assert(ctrl.reload_phase() == ReloadPhase::Idle);

    std::cout << "test_phase_transitions passed.\n";
}

// ---------------------------------------------------------------------------
// Test: IK offset is zero at idle, non-zero during reload phases
// ---------------------------------------------------------------------------

void test_ik_offset_nonzero_during_reload() {
    WeaponAnimationController ctrl;
    ctrl.reset();

    auto snap = make_snapshot(0, 25.0F, 50.0F, 150, false);
    auto input = make_input(true);

    // Verify IK offset is zero before reload
    ctrl.tick(0.0F, snap, input);
    assert(ik_at_zero(ctrl.reload_ik_offset()));

    // Start reload
    ctrl.tick(0.016F, snap, input);

    // IK offset should be non-zero during GrabMag (hand moving to mag)
    const float* ik = ctrl.reload_ik_offset();
    bool any_nonzero = !approx(ik[0], 0.0F) || !approx(ik[1], 0.0F) || !approx(ik[2], 0.0F);
    assert(any_nonzero);

    std::cout << "test_ik_offset_nonzero_during_reload passed.\n";
}

// ---------------------------------------------------------------------------
// Test: IK offset reaches magazine position during RemoveMag phase
// ---------------------------------------------------------------------------

void test_ik_offset_reaches_mag_pos() {
    WeaponAnimationController ctrl;
    ctrl.reset();

    auto snap = make_snapshot(0, 25.0F, 50.0F, 150, false);
    auto input = make_input(true);

    // Start reload and tick far enough to reach RemoveMag phase
    ctrl.tick(0.0F, snap, input);
    ctrl.tick(0.016F, snap, input);

    auto snap_reload = make_snapshot(0, 25.0F, 50.0F, 150, true);

    // Tick to 35% progress (~0.70s) — well into RemoveMag phase (0.18-0.45)
    ctrl.tick(0.68F, snap_reload, input);

    // At RemoveMag, IK offset should be at the full magazine position
    // (hand held at magazine)
    const auto& rd = kWeaponReloadData[0];
    const float* ik = ctrl.reload_ik_offset();

    // IK should be close to mag_pos (slight tolerance)
    assert(approx(ik[0], rd.mag_pos_x, 0.05F));
    assert(approx(ik[1], rd.mag_pos_y, 0.05F));
    assert(approx(ik[2], rd.mag_pos_z, 0.05F));

    std::cout << "test_ik_offset_reaches_mag_pos passed.\n";
}

// ---------------------------------------------------------------------------
// Test: Reload completes when timer expires
// ---------------------------------------------------------------------------

void test_reload_completes_on_timer_expiry() {
    WeaponAnimationController ctrl;
    ctrl.reset();

    auto snap = make_snapshot(0, 25.0F, 50.0F, 150, false);
    auto input = make_input(true);

    ctrl.tick(0.0F, snap, input);
    ctrl.tick(0.016F, snap, input);
    assert(ctrl.reload_phase() != ReloadPhase::Idle);

    // Tick past the full reload duration (2.0s for AR-15)
    auto snap_reload = make_snapshot(0, 25.0F, 50.0F, 150, true);
    ctrl.tick(3.0F, snap_reload, input);

    assert(ctrl.reload_phase() == ReloadPhase::Idle);
    assert(ctrl.reload_normalized() == 0.0F);
    assert(ik_at_zero(ctrl.reload_ik_offset()));

    std::cout << "test_reload_completes_on_timer_expiry passed.\n";
}

// ---------------------------------------------------------------------------
// Test: Reload completes early when runtime stops reloading
// ---------------------------------------------------------------------------

void test_reload_early_completion_on_runtime_sync() {
    WeaponAnimationController ctrl;
    ctrl.reset();

    auto snap = make_snapshot(0, 25.0F, 50.0F, 150, false);
    auto input = make_input(true);

    ctrl.tick(0.0F, snap, input);
    ctrl.tick(0.016F, snap, input);
    assert(ctrl.reload_phase() != ReloadPhase::Idle);

    // Tick with is_reloading = false — runtime says reload is done early
    auto snap_done = make_snapshot(0, 50.0F, 50.0F, 147, false);
    ctrl.tick(0.1F, snap_done, input);

    // Controller should detect runtime completion and stop animation
    assert(ctrl.reload_phase() == ReloadPhase::Idle &&
           "Reload should complete when runtime stops reloading");
    assert(ik_at_zero(ctrl.reload_ik_offset()));

    std::cout << "test_reload_early_completion_on_runtime_sync passed.\n";
}

// ---------------------------------------------------------------------------
// Test: Reload does NOT start when magazine is full
// ---------------------------------------------------------------------------

void test_no_reload_when_magazine_full() {
    WeaponAnimationController ctrl;
    ctrl.reset();

    // Full magazine + reload_pressed
    auto snap = make_snapshot(0, 50.0F, 50.0F, 150, false);
    auto input = make_input(true);

    ctrl.tick(0.0F, snap, input);
    ctrl.tick(0.016F, snap, input);

    // Should NOT start reload
    assert(ctrl.reload_phase() == ReloadPhase::Idle);

    std::cout << "test_no_reload_when_magazine_full passed.\n";
}

// ---------------------------------------------------------------------------
// Test: Reload does NOT start when no reserve ammo
// ---------------------------------------------------------------------------

void test_no_reload_when_no_reserve() {
    WeaponAnimationController ctrl;
    ctrl.reset();

    auto snap = make_snapshot(0, 25.0F, 50.0F, /*reserve=*/0, false);
    auto input = make_input(true);

    ctrl.tick(0.0F, snap, input);
    ctrl.tick(0.016F, snap, input);

    assert(ctrl.reload_phase() == ReloadPhase::Idle);

    std::cout << "test_no_reload_when_no_reserve passed.\n";
}

// ---------------------------------------------------------------------------
// Test: Per-weapon reload data differs between weapon types
// ---------------------------------------------------------------------------

void test_per_weapon_reload_data_differs() {
    const auto& ar15 = kWeaponReloadData[0];
    const auto& sg   = kWeaponReloadData[1];
    const auto& rl   = kWeaponReloadData[2];

    // Phase timing fractions should differ between weapon types
    // (at minimum, Shotgun should have longer remove/insert than AR-15)
    assert(ar15.grab_end < sg.grab_end);
    assert(ar15.remove_end < sg.remove_end);

    // Tilt angles should differ
    assert(ar15.tilt_pitch_deg != sg.tilt_pitch_deg);
    assert(sg.tilt_pitch_deg != rl.tilt_pitch_deg);

    std::cout << "test_per_weapon_reload_data_differs passed.\n";
}

// ---------------------------------------------------------------------------
// Test: Weapon switch resets reload state
// ---------------------------------------------------------------------------

void test_weapon_switch_resets_reload() {
    WeaponAnimationController ctrl;
    ctrl.reset();

    // Start reload on AR-15 (index 0)
    auto snap_ar = make_snapshot(0, 25.0F, 50.0F, 150, true);
    auto input = make_input(true);

    ctrl.tick(0.0F, snap_ar, input);
    ctrl.tick(0.016F, snap_ar, input);
    assert(ctrl.reload_phase() != ReloadPhase::Idle);

    // Switch to Shotgun (index 1) — should reset reload
    auto snap_sg = make_snapshot(1, 4.0F, 8.0F, 32, false);
    ctrl.tick(0.016F, snap_sg, input);
    assert(ctrl.reload_phase() == ReloadPhase::Idle);

    std::cout << "test_weapon_switch_resets_reload passed.\n";
}

// ---------------------------------------------------------------------------
// Test: Normalized progress increases monotonically
// ---------------------------------------------------------------------------

void test_reload_normalized_increases() {
    WeaponAnimationController ctrl;
    ctrl.reset();

    auto snap = make_snapshot(0, 25.0F, 50.0F, 150, false);
    auto input = make_input(true);

    ctrl.tick(0.0F, snap, input);
    ctrl.tick(0.016F, snap, input);

    float prev = ctrl.reload_normalized();
    auto snap_reload = make_snapshot(0, 25.0F, 50.0F, 150, true);

    for (int i = 0; i < 10; ++i) {
        ctrl.tick(0.1F, snap_reload, input);
        float cur = ctrl.reload_normalized();
        assert(cur >= prev);
        prev = cur;
    }

    std::cout << "test_reload_normalized_increases passed.\n";
}

// ---------------------------------------------------------------------------
// Test: Reload animation layers compose correctly with sway/bob/recoil
// ---------------------------------------------------------------------------

void test_reload_composes_with_other_layers() {
    WeaponAnimationController ctrl;
    ctrl.reset();

    // Verify the controller produces a valid transform during reload
    auto snap = make_snapshot(0, 25.0F, 50.0F, 150, false);
    auto input = make_input(true);

    ctrl.tick(0.0F, snap, input);
    ctrl.tick(0.016F, snap, input);
    assert(ctrl.has_transform());

    // Transform should be a valid 4x4 matrix (identity-like with non-zero diagonal)
    const auto& m = ctrl.transform();
    bool valid_diag = !approx(m[0], 0.0F) && !approx(m[5], 0.0F) && !approx(m[10], 0.0F) && !approx(m[15], 0.0F);
    assert(valid_diag && "Transform should have valid diagonal elements");

    std::cout << "test_reload_composes_with_other_layers passed.\n";
}

} // namespace

int main() {
    // Profile data integrity
    test_profiles_have_reload_data();
    test_per_weapon_reload_data_differs();

    // Reload start conditions
    test_reload_starts_on_input();
    test_reload_starts_on_runtime_sync();
    test_no_reload_when_magazine_full();
    test_no_reload_when_no_reserve();

    // Phase transitions
    test_phase_transitions();
    test_reload_normalized_increases();

    // IK offsets
    test_ik_offset_nonzero_during_reload();
    test_ik_offset_reaches_mag_pos();

    // Completion conditions
    test_reload_completes_on_timer_expiry();
    test_reload_early_completion_on_runtime_sync();

    // Weapon switch
    test_weapon_switch_resets_reload();

    // Composition with other layers
    test_reload_composes_with_other_layers();

    std::cout << "\nAll weapon animation controller tests passed.\n";
    return 0;
}
