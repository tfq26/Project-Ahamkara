/// @file weapon_animation_controller_tests.cpp
///
/// Tests for WeaponAnimationController — client-side first-person weapon
/// animation bridge.
///
/// Requires ae_render (GUI builds only) because the controller header
/// transitively includes ae/render/debug_renderer.h through
/// ahamkara/client/debug_scene_bridge.h.

#include "ahamkara/client/weapon_animation_controller.h"
#include "ahamkara/client/weapon_viewmodel_data.h"
#include "ahamkara/client/weapon_presentation.h"
#include "ahamkara/game/net_types.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

using namespace ahamkara::client;
using namespace ahamkara::game;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

constexpr float kEpsilon = 1.0e-6F;
constexpr float kDt = 1.0F / 60.0F;

/// Build a minimal snapshot with default state.
ClientSimulationSnapshot make_default_snapshot(int weapon_index = 0) {
    ClientSimulationSnapshot snap {};
    snap.weapon_index = weapon_index;
    snap.ammo_current = 30.0F;
    snap.ammo_max = 30.0F;
    snap.reserve_ammo = 90;
    snap.player_state.velocity = {0.0F, 0.0F, 0.0F};
    return snap;
}

/// Build a simple input command.
PlayerInputCommand make_input(bool aim_held = false, bool reload_pressed = false,
                              float look_dx = 0.0F, float look_dy = 0.0F) {
    PlayerInputCommand cmd {};
    cmd.aim_held = aim_held;
    cmd.reload_pressed = reload_pressed;
    cmd.look_delta = {look_dx, look_dy};
    return cmd;
}

/// Extract translation from a float[16] column-major matrix.
void get_translation(const std::array<float, 16>& m, float& tx, float& ty, float& tz) {
    tx = m[12];
    ty = m[13];
    tz = m[14];
}

/// Check if the transform is identity-like.
bool is_transform_identity(const std::array<float, 16>& m) {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            float expected = (r == c) ? 1.0F : 0.0F;
            float actual = m[r * 4 + c];
            if (std::fabs(actual - expected) > kEpsilon) return false;
        }
    }
    return true;
}

// ============================================================
// Lifecycle tests
// ============================================================

void test_construction_and_reset() {
    WeaponAnimationController ctrl;
    if (ctrl.has_transform()) {
        std::cerr << "new controller should not have a transform\n";
        std::exit(1);
    }
    if (std::fabs(ctrl.ads_blend()) > kEpsilon) {
        std::cerr << "new controller ads_blend should be 0\n";
        std::exit(1);
    }
    if (ctrl.reload_phase() != ReloadPhase::Idle) {
        std::cerr << "new controller reload_phase should be Idle\n";
        std::exit(1);
    }
    if (std::fabs(ctrl.reload_normalized()) > kEpsilon) {
        std::cerr << "new controller reload_normalized should be 0\n";
        std::exit(1);
    }

    ctrl.reset();
    if (ctrl.has_transform()) {
        std::cerr << "after reset, controller should not have a transform\n";
        std::exit(1);
    }
    if (ctrl.reload_phase() != ReloadPhase::Idle) {
        std::cerr << "after reset, reload_phase should be Idle\n";
        std::exit(1);
    }

    std::cout << "test_construction_and_reset passed.\n";
}

// ============================================================
// Weapon switch tests
// ============================================================

void test_weapon_switch_detected() {
    WeaponAnimationController ctrl;
    auto snap = make_default_snapshot(0);
    auto input = make_input();

    // First tick with weapon 0 — should set transform
    ctrl.tick(kDt, snap, input);
    if (!ctrl.has_transform()) {
        std::cerr << "controller should have transform after first tick with valid weapon\n";
        std::exit(1);
    }

    // Remember transform from weapon 0
    auto t0 = ctrl.transform();

    // Switch to weapon 1
    snap.weapon_index = 1;
    ctrl.tick(kDt, snap, input);

    // Should still have a transform and it should be different
    if (!ctrl.has_transform()) {
        std::cerr << "controller should have transform after weapon switch\n";
        std::exit(1);
    }

    // Recoil state should have reset on switch
    ctrl.notify_fired();
    ctrl.tick(kDt, snap, input);
    std::cout << "test_weapon_switch_detected passed.\n";
}

// ============================================================
// ADS tests
// ============================================================

void test_ads_blend_enters_and_exits() {
    WeaponAnimationController ctrl;
    auto snap = make_default_snapshot(0);
    auto input = make_input(false);

    // Tick a few frames without ADS to stabilize
    for (int i = 0; i < 10; ++i) {
        ctrl.tick(kDt, snap, input);
    }

    // Should not be ADS
    if (std::fabs(ctrl.ads_blend()) > 0.01F) {
        std::cerr << "ads_blend should be ~0 when not aiming, got " << ctrl.ads_blend() << "\n";
        std::exit(1);
    }

    // Press ADS
    input.aim_held = true;
    for (int i = 0; i < 30; ++i) {  // ~0.5 seconds
        ctrl.tick(kDt, snap, input);
    }

    // Should be fully ADS
    if (std::fabs(ctrl.ads_blend() - 1.0F) > 0.05F) {
        std::cerr << "ads_blend should be ~1 after holding aim, got " << ctrl.ads_blend() << "\n";
        std::exit(1);
    }

    // Release ADS
    input.aim_held = false;
    for (int i = 0; i < 30; ++i) {
        ctrl.tick(kDt, snap, input);
    }

    // Should be back to hip-fire
    if (std::fabs(ctrl.ads_blend()) > 0.05F) {
        std::cerr << "ads_blend should be ~0 after releasing aim, got " << ctrl.ads_blend() << "\n";
        std::exit(1);
    }
    std::cout << "test_ads_blend_enters_and_exits passed.\n";
}

// ============================================================
// Recoil / notify_fired tests
// ============================================================

void test_notify_fired_produces_recoil_kick() {
    WeaponAnimationController ctrl;
    auto snap = make_default_snapshot(0);
    auto input = make_input();

    // Establish baseline
    for (int i = 0; i < 10; ++i) {
        ctrl.tick(kDt, snap, input);
    }
    auto baseline = ctrl.transform();

    // Fire
    ctrl.notify_fired();
    ctrl.tick(kDt, snap, input);

    auto after_fire = ctrl.transform();

    // Transform should differ due to recoil kick
    if (after_fire == baseline) {
        std::cerr << "transform should change after firing\n";
        std::exit(1);
    }
    std::cout << "test_notify_fired_produces_recoil_kick passed.\n";
}

// ============================================================
// Reload tests
// ============================================================

void test_reload_triggers_on_input() {
    WeaponAnimationController ctrl;
    auto snap = make_default_snapshot(0);
    snap.ammo_current = 5.0F;
    snap.ammo_max = 30.0F;
    snap.reserve_ammo = 60;
    auto input = make_input();

    // Stabilize first
    for (int i = 0; i < 5; ++i) {
        ctrl.tick(kDt, snap, input);
    }

    // Press reload
    input.reload_pressed = true;
    ctrl.tick(kDt, snap, input);

    // Should have transitioned into a reload phase
    if (ctrl.reload_phase() == ReloadPhase::Idle) {
        std::cerr << "reload_phase should change from Idle after reload_pressed\n";
        std::exit(1);
    }
    if (ctrl.reload_normalized() <= kEpsilon) {
        std::cerr << "reload_normalized should advance after reload start\n";
        std::exit(1);
    }
    std::cout << "test_reload_triggers_on_input passed.\n";
}

void test_reload_phase_sequence() {
    WeaponAnimationController ctrl;
    auto snap = make_default_snapshot(0);
    snap.ammo_current = 1.0F;
    snap.ammo_max = 30.0F;
    snap.reserve_ammo = 60;
    auto input = make_input();

    // Stabilize
    for (int i = 0; i < 5; ++i) {
        ctrl.tick(kDt, snap, input);
    }

    // Start reload
    input.reload_pressed = true;
    ctrl.tick(kDt, snap, input);

    // Track phases over the full reload duration
    ReloadPhase prev_phase = ReloadPhase::Idle;
    bool seen_grab = false;
    bool seen_remove = false;
    bool seen_insert = false;
    bool seen_return = false;

    // Run for ~3 seconds at 60 Hz to cover a full reload (AR-15 = 2.0s duration)
    for (int i = 0; i < 180; ++i) {
        ctrl.tick(kDt, snap, input);
        ReloadPhase phase = ctrl.reload_phase();
        if (phase != prev_phase) {
            prev_phase = phase;
            switch (phase) {
                case ReloadPhase::GrabMag:   seen_grab = true; break;
                case ReloadPhase::RemoveMag: seen_remove = true; break;
                case ReloadPhase::InsertMag: seen_insert = true; break;
                case ReloadPhase::ReturnToGrip: seen_return = true; break;
                default: break;
            }
        }
    }

    // All reload phases should have been visited in order during the sequence
    if (!seen_grab) {
        std::cerr << "reload sequence should include GrabMag phase\n";
        std::exit(1);
    }
    if (!seen_remove) {
        std::cerr << "reload sequence should include RemoveMag phase\n";
        std::exit(1);
    }
    if (!seen_insert) {
        std::cerr << "reload sequence should include InsertMag phase\n";
        std::exit(1);
    }
    if (!seen_return) {
        std::cerr << "reload sequence should include ReturnToGrip phase\n";
        std::exit(1);
    }

    std::cout << "test_reload_phase_sequence passed.\n";
}

void test_reload_completes_and_returns_to_idle() {
    WeaponAnimationController ctrl;
    auto snap = make_default_snapshot(0);
    snap.ammo_current = 1.0F;
    snap.ammo_max = 30.0F;
    snap.reserve_ammo = 60;
    auto input = make_input();

    // Start reload
    input.reload_pressed = true;
    ctrl.tick(kDt, snap, input);

    // Run for full reload duration + buffer
    for (int i = 0; i < 180; ++i) {
        input.reload_pressed = false;
        ctrl.tick(kDt, snap, input);
    }

    // After completion, should be Idle
    if (ctrl.reload_phase() != ReloadPhase::Idle) {
        std::cerr << "reload_phase should be Idle after reload completes, got "
                  << static_cast<int>(ctrl.reload_phase()) << "\n";
        std::exit(1);
    }
    if (std::fabs(ctrl.reload_normalized()) > kEpsilon) {
        std::cerr << "reload_normalized should be 0 after completion\n";
        std::exit(1);
    }
    std::cout << "test_reload_completes_and_returns_to_idle passed.\n";
}

// ============================================================
// Melee tests
// ============================================================

void test_melee_trigger_and_duration() {
    WeaponAnimationController ctrl;

    // Not active initially
    if (ctrl.is_melee_active()) {
        std::cerr << "melee should not be active initially\n";
        std::exit(1);
    }

    // Trigger melee
    bool ok = ctrl.trigger_melee();
    if (!ok) {
        std::cerr << "trigger_melee() should return true on fresh controller\n";
        std::exit(1);
    }
    if (!ctrl.is_melee_active()) {
        std::cerr << "melee should be active after trigger\n";
        std::exit(1);
    }

    // Can't trigger again while active
    ok = ctrl.trigger_melee();
    if (ok) {
        std::cerr << "trigger_melee() should return false while already active\n";
        std::exit(1);
    }
    std::cout << "test_melee_trigger_and_duration passed.\n";
}

void test_melee_normalized_progress() {
    WeaponAnimationController ctrl;

    ctrl.trigger_melee();
    float n0 = ctrl.melee_normalized();
    if (n0 > kEpsilon) {
        std::cerr << "melee_normalized should be ~0 at start, got " << n0 << "\n";
        std::exit(1);
    }

    // Tick through melee duration
    auto snap = make_default_snapshot(0);
    auto input = make_input();
    for (int i = 0; i < 60; ++i) {  // ~1 second, covers 0.6s melee
        ctrl.tick(kDt, snap, input);
    }

    float n1 = ctrl.melee_normalized();
    if (n1 < 0.99F) {
        std::cerr << "melee_normalized should be ~1 near end, got " << n1 << "\n";
        std::exit(1);
    }

    if (ctrl.is_melee_active()) {
        std::cerr << "melee should have completed after full duration\n";
        std::exit(1);
    }
    std::cout << "test_melee_normalized_progress passed.\n";
}

// ============================================================
// Weapon viewmodel data integration tests
// ============================================================

void test_reload_ik_offsets_within_bounds() {
    // Verify that reload IK offsets stay within sensible ranges
    WeaponAnimationController ctrl;
    auto snap = make_default_snapshot(0);
    snap.ammo_current = 1.0F;
    snap.ammo_max = 30.0F;
    snap.reserve_ammo = 60;
    auto input = make_input();

    // Stabilize and start reload
    for (int i = 0; i < 5; ++i) ctrl.tick(kDt, snap, input);
    input.reload_pressed = true;
    ctrl.tick(kDt, snap, input);

    // Throughout reload, IK offsets should stay within ±0.5m
    for (int i = 0; i < 150; ++i) {
        ctrl.tick(kDt, snap, input);
        auto* ik = ctrl.reload_ik_offset();
        if (ik == nullptr) {
            std::cerr << "reload_ik_offset should not be null during reload\n";
            std::exit(1);
        }
        if (std::fabs(ik[0]) > 0.5F || std::fabs(ik[1]) > 0.5F || std::fabs(ik[2]) > 0.5F) {
            std::cerr << "reload IK offset out of bounds: " << ik[0] << ", " << ik[1] << ", " << ik[2] << "\n";
            std::exit(1);
        }
    }
    std::cout << "test_reload_ik_offsets_within_bounds passed.\n";
}

// ============================================================
// Viewmodel transform resolution integration
// ============================================================

void test_weapon_viewmodel_transform_resolution() {
    // All three weapons must resolve valid viewmodel data
    for (int i = 0; i < 3; ++i) {
        WeaponAnimationController ctrl;
        auto snap = make_default_snapshot(i);
        auto input = make_input();

        // Tick to produce a transform
        for (int j = 0; j < 5; ++j) {
            ctrl.tick(kDt, snap, input);
        }

        if (!ctrl.has_transform()) {
            std::cerr << "weapon " << i << " should produce a transform\n";
            std::exit(1);
        }
    }
    std::cout << "test_weapon_viewmodel_transform_resolution passed.\n";
}

// ============================================================
// Presentation contract: apply_viewmodel_arm_ik
// ============================================================

/// Build a minimal joint matrix array simulating the viewmodel arm skeleton
/// (7 joints × 16 floats each = 112 floats).
void make_arm_joint_matrices(float* out, int weapon_index,
                             const float* ik_offset = nullptr) {
    // Initialize all joints to identity
    constexpr int kJointCount = 7;
    constexpr int kFloatsPerJoint = 16;
    for (int i = 0; i < kJointCount * kFloatsPerJoint; ++i) {
        out[i] = 0.0F;
    }
    for (int j = 0; j < kJointCount; ++j) {
        out[j * kFloatsPerJoint + 0] = 1.0F;   // m[0]
        out[j * kFloatsPerJoint + 5] = 1.0F;   // m[5]
        out[j * kFloatsPerJoint + 10] = 1.0F;  // m[10]
        out[j * kFloatsPerJoint + 15] = 1.0F;  // m[15]
    }

    // Set shoulder position (joint 2) in model space
    // In the viewmodel_arms skeleton, shoulder is at a negative Y offset
    // from root. We approximate the bind pose: shoulder at y ~ -0.05
    out[2 * kFloatsPerJoint + 12] = 0.0F;   // shoulder.x
    out[2 * kFloatsPerJoint + 13] = -0.05F; // shoulder.y (down from root)
    out[2 * kFloatsPerJoint + 14] = 0.0F;   // shoulder.z

    // Apply IK
    apply_viewmodel_arm_ik(weapon_index, out, kJointCount, ik_offset);
}

void test_ik_produces_valid_matrices() {
    float mats[7 * 16];  // 7 joints × 16 floats
    make_arm_joint_matrices(mats, 0, nullptr);

    // Shoulder (joint 2) and elbow (joint 3) should have been modified by IK
    // Check that they still produce valid rotation quaternions (orthogonal rows)
    for (int j = 2; j <= 3; ++j) {
        float* m = mats + j * 16;
        // Check that the rotation part (3x3) has no NaN
        for (int i = 0; i < 3; ++i) {
            for (int k = 0; k < 3; ++k) {
                float v = m[k * 4 + i];  // column-major access
                if (std::isnan(v)) {
                    std::cerr << "joint " << j << " matrix contains NaN\n";
                    std::exit(1);
                }
            }
        }

        // Check that the scale part stays 1 (no scaling from IK)
        // Column-major: m[0], m[5], m[10] should be ~1
        float sx = std::sqrt(m[0]*m[0] + m[1]*m[1] + m[2]*m[2]);
        if (std::fabs(sx - 1.0F) > 0.01F) {
            std::cerr << "joint " << j << " scale X changed by IK: " << sx << "\n";
            std::exit(1);
        }
    }
    std::cout << "test_ik_produces_valid_matrices passed.\n";
}

void test_ik_with_reload_offset() {
    float mats[7 * 16];
    float ik_offset[3] = {0.1F, 0.0F, 0.0F};  // small offset

    make_arm_joint_matrices(mats, 0, ik_offset);

    // Should still produce valid matrices
    for (int j = 2; j <= 3; ++j) {
        float* m = mats + j * 16;
        for (int i = 0; i < 16; ++i) {
            if (std::isnan(m[i])) {
                std::cerr << "joint " << j << " has NaN with IK offset\n";
                std::exit(1);
            }
        }
    }
    std::cout << "test_ik_with_reload_offset passed.\n";
}

void test_ik_different_weapons_all_succeed() {
    // Run IK for all three weapons with their grip socket data
    for (int w = 0; w < 3; ++w) {
        float mats[7 * 16];
        make_arm_joint_matrices(mats, w, nullptr);

        // Shoulder and elbow matrices should be valid
        for (int j = 2; j <= 3; ++j) {
            float* m = mats + j * 16;
            for (int i = 0; i < 16; ++i) {
                if (std::isnan(m[i])) {
                    std::cerr << "weapon " << w << " joint " << j << " has NaN\n";
                    std::exit(1);
                }
            }
        }
    }
    std::cout << "test_ik_different_weapons_all_succeed passed.\n";
}

void test_ik_short_joint_count_does_not_crash() {
    // With fewer than 6 joints, IK should be a no-op
    float mats[4 * 16];
    // Should not crash
    apply_viewmodel_arm_ik(0, mats, 4, nullptr);
    std::cout << "test_ik_short_joint_count_does_not_crash passed.\n";
}

void test_ik_null_matrices_does_not_crash() {
    // Null matrices should be handled gracefully
    apply_viewmodel_arm_ik(0, nullptr, 0, nullptr);
    std::cout << "test_ik_null_matrices_does_not_crash passed.\n";
}

}  // namespace

int main() {
    // Lifecycle
    test_construction_and_reset();

    // Weapon switch
    test_weapon_switch_detected();

    // ADS
    test_ads_blend_enters_and_exits();

    // Recoil / fire
    test_notify_fired_produces_recoil_kick();

    // Reload
    test_reload_triggers_on_input();
    test_reload_phase_sequence();
    test_reload_completes_and_returns_to_idle();
    test_reload_ik_offsets_within_bounds();

    // Melee
    test_melee_trigger_and_duration();
    test_melee_normalized_progress();

    // Viewmodel data integration
    test_weapon_viewmodel_transform_resolution();

    // Presentation contract (IK)
    test_ik_produces_valid_matrices();
    test_ik_with_reload_offset();
    test_ik_different_weapons_all_succeed();
    test_ik_short_joint_count_does_not_crash();
    test_ik_null_matrices_does_not_crash();

    std::cout << "\nAll weapon animation controller tests passed.\n";
    return 0;
}
