#include "ae/animation/character_weapon.h"
#include "ae/skeleton/types.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

using namespace ae::animation;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

constexpr float kEpsilon = 1.0e-6F;
constexpr float kDt = 1.0F / 60.0F;  // 60 Hz timestep

/// Check that a 4x4 matrix is identity (within epsilon).
bool is_identity(const ae::skeleton::Mat4& m) {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            float expected = (r == c) ? 1.0F : 0.0F;
            float actual = m.m[r * 4 + c];  // column-major
            if (std::fabs(actual - expected) > kEpsilon) return false;
        }
    }
    return true;
}

/// Extract translation from a Mat4 (column-major: m[12], m[13], m[14]).
void get_translation(const ae::skeleton::Mat4& m, float& tx, float& ty, float& tz) {
    tx = m.m[12];
    ty = m.m[13];
    tz = m.m[14];
}

// ============================================================
// Sway layer tests
// ============================================================

void test_sway_layer_idle_oscillation_with_no_input() {
    WeaponAnimState state {};
    WeaponAnimConfig config {};
    config.sway_amplitude = 0.01F;     // small idle oscillation
    ae::skeleton::Mat4 offset;

    // With zero look delta and dt > 0, the sway layer produces a small idle
    // oscillation (sin/cos of sway_phase).  This is expected behaviour.
    evaluate_sway_layer(state, config, kDt, 0.0F, 0.0F, 0.0F, offset);

    float tx, ty, tz;
    get_translation(offset, tx, ty, tz);

    // The idle sway should be within the amplitude range.
    if (std::fabs(tx) > config.sway_amplitude || std::fabs(ty) > config.sway_amplitude) {
        std::cerr << "idle sway exceeded amplitude: (" << tx << ", " << ty << ")\n";
        std::exit(1);
    }

    // With zero amplitude, the sway should be identity.
    config.sway_amplitude = 0.0F;
    evaluate_sway_layer(state, config, kDt, 0.0F, 0.0F, 0.0F, offset);
    if (!is_identity(offset)) {
        std::cerr << "sway should be identity with zero amplitude\n";
        std::exit(1);
    }
    std::cout << "test_sway_layer_idle_oscillation_with_no_input passed.\n";
}

void test_sway_layer_accumulates_look_input() {
    WeaponAnimState state {};
    WeaponAnimConfig config {};
    config.sway_amplitude = 0.01F;
    config.sway_frequency = 1.0F;
    config.sway_damping = 3.0F;
    ae::skeleton::Mat4 offset;

    // Feed look delta repeatedly — sway should accumulate.
    float total_dx = 0.0F;
    for (int i = 0; i < 10; ++i) {
        evaluate_sway_layer(state, config, kDt, 0.5F, 0.3F, 0.0F, offset);
        total_dx += 0.5F * kDt * 0.5F;  // accumulated velocity
    }

    float tx, ty, tz;
    get_translation(offset, tx, ty, tz);

    // After sustained look input, sway should produce a non-zero offset.
    if (std::fabs(tx) < kEpsilon && std::fabs(ty) < kEpsilon) {
        std::cerr << "sway should produce non-zero offset after look input\n";
        std::exit(1);
    }

    std::cout << "test_sway_layer_accumulates_look_input passed.\n";
}

void test_sway_layer_ads_reduces_amplitude() {
    WeaponAnimState state {};
    WeaponAnimConfig config {};
    config.sway_amplitude = 0.01F;
    config.sway_frequency = 1.0F;
    config.sway_damping = 3.0F;
    config.ads_sway_multiplier = 0.3F;
    ae::skeleton::Mat4 offset_hip, offset_ads;

    // Same input, but one with full ADS
    for (int i = 0; i < 10; ++i) {
        evaluate_sway_layer(state, config, kDt, 0.5F, 0.3F, 0.0F, offset_hip);
    }

    // Reset state for ADS test
    WeaponAnimState ads_state {};
    for (int i = 0; i < 10; ++i) {
        evaluate_sway_layer(ads_state, config, kDt, 0.5F, 0.3F, 1.0F, offset_ads);
    }

    float tx_hip, ty_hip, tz_hip;
    float tx_ads, ty_ads, tz_ads;
    get_translation(offset_hip, tx_hip, ty_hip, tz_hip);
    get_translation(offset_ads, tx_ads, ty_ads, tz_ads);

    // ADS sway amplitude should be visibly smaller than hip
    float hip_mag = std::sqrt(tx_hip * tx_hip + ty_hip * ty_hip);
    float ads_mag = std::sqrt(tx_ads * tx_ads + ty_ads * ty_ads);

    if (ads_mag >= hip_mag) {
        std::cerr << "ADS sway amplitude should be less than hip\n";
        std::exit(1);
    }
    std::cout << "test_sway_layer_ads_reduces_amplitude passed.\n";
}

void test_sway_layer_damping_decays_velocity() {
    WeaponAnimState state {};
    WeaponAnimConfig config {};
    config.sway_amplitude = 0.0F;      // Disable idle oscillation for this test
    config.sway_frequency = 1.0F;
    config.sway_damping = 10.0F;       // Aggressive damping
    ae::skeleton::Mat4 offset;

    // Inject sway velocity by feeding look input.
    for (int i = 0; i < 5; ++i) {
        evaluate_sway_layer(state, config, kDt, 10.0F, 0.0F, 0.0F, offset);
    }

    // Now stop input and run many frames — sway should decay toward zero.
    for (int i = 0; i < 120; ++i) {  // 2 seconds at 60 Hz
        evaluate_sway_layer(state, config, kDt, 0.0F, 0.0F, 0.0F, offset);
    }

    float tx, ty, tz;
    get_translation(offset, tx, ty, tz);

    if (std::fabs(tx) > 0.001F || std::fabs(ty) > 0.001F) {
        std::cerr << "sway should decay near zero after damping: (" << tx << ", " << ty << ")\n";
        std::exit(1);
    }
    std::cout << "test_sway_layer_damping_decays_velocity passed.\n";
}

// ============================================================
// Bob layer tests
// ============================================================

void test_bob_layer_identity_when_not_moving() {
    WeaponAnimState state {};
    WeaponAnimConfig config {};
    ae::skeleton::Mat4 offset;

    evaluate_bob_layer(state, config, kDt, 0.0F, false, offset);

    if (!is_identity(offset)) {
        std::cerr << "bob should be identity when not moving\n";
        std::exit(1);
    }
    std::cout << "test_bob_layer_identity_when_not_moving passed.\n";
}

void test_bob_layer_produces_oscillation() {
    WeaponAnimState state {};
    WeaponAnimConfig config {};
    config.bob_amplitude_vertical = 0.01F;
    config.bob_amplitude_horizontal = 0.005F;
    config.bob_frequency_walk = 2.0F;
    config.bob_frequency_sprint = 3.5F;
    ae::skeleton::Mat4 offset;

    // Walk at moderate speed
    for (int i = 0; i < 30; ++i) {
        evaluate_bob_layer(state, config, kDt, 3.0F, true, offset);
    }

    float tx, ty, tz;
    get_translation(offset, tx, ty, tz);

    // Bob should produce a noticeable vertical offset
    if (std::fabs(ty) < kEpsilon) {
        std::cerr << "bob vertical should be non-zero when walking\n";
        std::exit(1);
    }

    // Bob phase should have advanced
    if (state.bob_phase <= kEpsilon) {
        std::cerr << "bob phase should advance when moving\n";
        std::exit(1);
    }
    std::cout << "test_bob_layer_produces_oscillation passed.\n";
}

void test_bob_layer_sprint_faster_than_walk() {
    WeaponAnimState walk_state {};
    WeaponAnimState sprint_state {};
    WeaponAnimConfig config {};
    config.bob_amplitude_vertical = 0.01F;
    config.bob_amplitude_horizontal = 0.005F;
    config.bob_frequency_walk = 2.0F;
    config.bob_frequency_sprint = 4.0F;
    ae::skeleton::Mat4 walk_offset, sprint_offset;

    // Walk for 30 frames
    for (int i = 0; i < 30; ++i) {
        evaluate_bob_layer(walk_state, config, kDt, 3.0F, true, walk_offset);
    }

    // Sprint for 30 frames
    for (int i = 0; i < 30; ++i) {
        evaluate_bob_layer(sprint_state, config, kDt, 6.0F, true, sprint_offset);
    }

    // Sprinting should advance phase more than walking
    if (sprint_state.bob_phase <= walk_state.bob_phase) {
        std::cerr << "sprint bob phase should advance faster than walk\n";
        std::exit(1);
    }
    std::cout << "test_bob_layer_sprint_faster_than_walk passed.\n";
}

// ============================================================
// Recoil kick layer tests
// ============================================================

void test_recoil_kick_identity_when_not_firing() {
    WeaponAnimState state {};
    WeaponAnimConfig config {};
    ae::skeleton::Mat4 offset;

    evaluate_recoil_kick_layer(state, config, kDt, offset);

    if (!is_identity(offset)) {
        std::cerr << "recoil kick should be identity when not firing\n";
        std::exit(1);
    }
    std::cout << "test_recoil_kick_identity_when_not_firing passed.\n";
}

void test_fire_weapon_kick_triggers_offset() {
    WeaponAnimState state {};
    WeaponAnimConfig config {};
    ae::skeleton::Mat4 offset;

    // Trigger a kick
    fire_weapon_kick(state);

    // Evaluate — should produce a non-identity offset
    evaluate_recoil_kick_layer(state, config, kDt, offset);

    if (is_identity(offset)) {
        std::cerr << "recoil kick should produce offset after fire\n";
        std::exit(1);
    }

    float tx, ty, tz;
    get_translation(offset, tx, ty, tz);

    // Kick should push the weapon up (negative Y in our convention)
    if (ty >= 0.0F) {
        std::cerr << "recoil kick should push weapon up (ty < 0), got ty=" << ty << "\n";
        std::exit(1);
    }
    std::cout << "test_fire_weapon_kick_triggers_offset passed.\n";
}

void test_recoil_kick_decays_over_time() {
    WeaponAnimState state {};
    WeaponAnimConfig config {};
    ae::skeleton::Mat4 offset;

    fire_weapon_kick(state);

    // First frame: visible kick
    evaluate_recoil_kick_layer(state, config, kDt, offset);
    float ty0;
    get_translation(offset, ty0, ty0, ty0);  // just need y
    {
        float tx, ty, tz;
        get_translation(offset, tx, ty, tz);
        ty0 = ty;
    }

    // Run many frames to let it decay
    for (int i = 0; i < 10; ++i) {
        evaluate_recoil_kick_layer(state, config, kDt, offset);
    }

    float tx, ty, tz;
    get_translation(offset, tx, ty, tz);

    // Should have decayed significantly
    if (std::fabs(ty) >= std::fabs(ty0)) {
        std::cerr << "recoil kick should decay over time: " << ty0 << " -> " << ty << "\n";
        std::exit(1);
    }

    // After enough frames, should return to identity
    for (int i = 0; i < 60; ++i) {
        evaluate_recoil_kick_layer(state, config, kDt, offset);
    }
    if (!is_identity(offset)) {
        std::cerr << "recoil kick should return to identity after decay\n";
        std::exit(1);
    }
    std::cout << "test_recoil_kick_decays_over_time passed.\n";
}

// ============================================================
// Composite weapon animation tests
// ============================================================

void test_evaluate_weapon_animation_composes_layers() {
    WeaponAnimState state {};
    WeaponAnimConfig config {};
    config.sway_amplitude = 0.0F;       // Disable idle oscillation for baseline
    config.bob_amplitude_vertical = 0.01F;
    config.ads_transition_time = 0.2F;
    ae::skeleton::Mat4 transform;

    // Not moving, not firing, no input — with zero sway amplitude the
    // composite transform should be identity.
    evaluate_weapon_animation(state, config, kDt, 0.0F, 0.0F, 0.0F, false, false, false, transform);
    if (!is_identity(transform)) {
        std::cerr << "composite transform should be identity with zero-amplitude sway and no inputs\n";
        std::exit(1);
    }

    // Now fire and move
    evaluate_weapon_animation(state, config, kDt, 3.0F, 0.5F, 0.0F, true, false, true, transform);

    // Should produce a non-identity transform
    float tx, ty, tz;
    get_translation(transform, tx, ty, tz);
    if (std::fabs(tx) < kEpsilon && std::fabs(ty) < kEpsilon && std::fabs(tz) < kEpsilon) {
        std::cerr << "composite transform should be non-identity with firing+moving\n";
        std::exit(1);
    }
    std::cout << "test_evaluate_weapon_animation_composes_layers passed.\n";
}

void test_ads_blend_transition() {
    WeaponAnimState state {};
    WeaponAnimConfig config {};
    config.ads_transition_time = 0.2F;  // 200ms transition
    ae::skeleton::Mat4 transform;

    // Start not ADS
    evaluate_weapon_animation(state, config, kDt, 0.0F, 0.0F, 0.0F, false, false, false, transform);
    if (std::fabs(state.ads_blend) > kEpsilon) {
        std::cerr << "ads_blend should be 0 when not aiming\n";
        std::exit(1);
    }

    // Enable ADS
    evaluate_weapon_animation(state, config, 0.1F, 0.0F, 0.0F, 0.0F, false, true, false, transform);
    if (state.ads_blend <= 0.0F) {
        std::cerr << "ads_blend should increase when aiming\n";
        std::exit(1);
    }
    // Not yet fully aimed after 100ms
    if (state.ads_blend >= 1.0F) {
        std::cerr << "ads_blend should not reach 1.0 before transition time\n";
        std::exit(1);
    }

    // Wait for full transition
    evaluate_weapon_animation(state, config, 0.2F, 0.0F, 0.0F, 0.0F, false, true, false, transform);
    if (std::fabs(state.ads_blend - 1.0F) > 0.01F) {
        std::cerr << "ads_blend should reach 1.0 after transition time, got " << state.ads_blend << "\n";
        std::exit(1);
    }

    // Disable ADS
    evaluate_weapon_animation(state, config, 0.1F, 0.0F, 0.0F, 0.0F, false, false, false, transform);
    if (state.ads_blend >= 1.0F) {
        std::cerr << "ads_blend should decrease when not aiming\n";
        std::exit(1);
    }
    std::cout << "test_ads_blend_transition passed.\n";
}

}  // namespace

int main() {
    // Sway
    test_sway_layer_idle_oscillation_with_no_input();
    test_sway_layer_accumulates_look_input();
    test_sway_layer_ads_reduces_amplitude();
    test_sway_layer_damping_decays_velocity();

    // Bob
    test_bob_layer_identity_when_not_moving();
    test_bob_layer_produces_oscillation();
    test_bob_layer_sprint_faster_than_walk();

    // Recoil kick
    test_recoil_kick_identity_when_not_firing();
    test_fire_weapon_kick_triggers_offset();
    test_recoil_kick_decays_over_time();

    // Composite
    test_evaluate_weapon_animation_composes_layers();
    test_ads_blend_transition();

    std::cout << "\nAll weapon animation layer tests passed.\n";
    return 0;
}
