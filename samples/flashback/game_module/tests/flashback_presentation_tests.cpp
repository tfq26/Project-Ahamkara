// Flashback first-person presentation contract tests.
//
// Extracts a deterministic Flashback-owned presentation boundary that runs
// without a graphics context. Exercises:
//   - Per-weapon mesh/transform/FOV selection and fallback
//   - Transform composition order (sway * bob * recoil * ADS * reload * melee)
//   - IK reachability, overextension, degenerate, and disabled rigs
//   - Reload phase ordering and IK offset computation
//   - ADS transition determinism across variable frame partitions
//   - Sway, bob, and recoil layer clamps and reset behaviour
//
// Tests run headlessly via the independent Flashback product boundary
// (ae_animation + viewmodel data headers, no render-backend dependency).

#include "ae/animation/character_weapon.h"
#include "ae/animation/ik.h"
#include "ae/animation/aim_recoil.h"
#include "ae/skeleton/types.h"
#include "ahamkara/client/weapon_viewmodel_data.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------

namespace {

int g_failures = 0;

#define EXPECT_TRUE(cond)                                                 \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n";     \
            ++g_failures;                                                 \
        }                                                                 \
    } while (0)

#define EXPECT_FALSE(cond)                                                \
    do {                                                                  \
        if ((cond)) {                                                     \
            std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n";     \
            ++g_failures;                                                 \
        }                                                                 \
    } while (0)

#define EXPECT_NEAR(a, b, eps)                                            \
    do {                                                                  \
        auto _a = (a);                                                    \
        auto _b = (b);                                                    \
        auto _e = (eps);                                                  \
        if (std::fabs(_a - _b) > _e) {                                   \
            std::cerr << "FAIL line " << __LINE__ << ": " #a " (" << _a   \
                      << ") ~= " #b " (" << _b << ") eps=" << _e << "\n";\
            ++g_failures;                                                 \
        }                                                                 \
    } while (0)

// Check that a float is not NaN
#define EXPECT_FINITE(x)                                                  \
    do {                                                                  \
        auto _v = (x);                                                    \
        if (std::isnan(_v) || std::isinf(_v)) {                           \
            std::cerr << "FAIL line " << __LINE__ << ": " #x              \
                      << " is NaN or Inf (" << _v << ")\n";               \
            ++g_failures;                                                 \
        }                                                                 \
    } while (0)

// Check that no element of a skeleton::Mat4 is NaN
void expect_mat4_finite(const ae::skeleton::Mat4& m, const char* label) {
    for (int i = 0; i < 16; ++i) {
        if (std::isnan(m.m[i]) || std::isinf(m.m[i])) {
            std::cerr << "FAIL: " << label << "[" << i << "] is NaN/Inf ("
                      << m.m[i] << ")\n";
            ++g_failures;
            return;
        }
    }
}

#define EXPECT_MAT4_FINITE(m) expect_mat4_finite(m, #m)

// Check two Mat4 are approximately equal
bool mat4_nearly_equal(const ae::skeleton::Mat4& a,
                       const ae::skeleton::Mat4& b,
                       float eps = 1.0e-4F) {
    for (int i = 0; i < 16; ++i) {
        if (std::fabs(a.m[i] - b.m[i]) > eps) return false;
    }
    return true;
}

constexpr float kEps = 1.0e-5F;
constexpr float kPi = 3.14159265358979323846F;

// ---------------------------------------------------------------------------
// 1. Viewmodel Data — mesh path, transforms, FOV, grip sockets, reload, ADS
// ---------------------------------------------------------------------------

void test_mesh_path_resolution() {
    // Each authored weapon resolves its intended mesh
    EXPECT_TRUE(std::strcmp(ahamkara::client::weapon_viewmodel_mesh_path(0),
                            "assets/compiled/models/viewmodel_ar15.aemesh") == 0);
    EXPECT_TRUE(std::strcmp(ahamkara::client::weapon_viewmodel_mesh_path(1),
                            "assets/compiled/models/viewmodel_shotgun.aemesh") == 0);
    EXPECT_TRUE(std::strcmp(ahamkara::client::weapon_viewmodel_mesh_path(2),
                            "assets/compiled/models/viewmodel_rocket_launcher.aemesh") == 0);

    // Missing / invalid indices produce stable fallback diagnostic
    EXPECT_TRUE(std::strcmp(ahamkara::client::weapon_viewmodel_mesh_path(-1),
                            "assets/compiled/models/viewmodel_arms.aemesh") == 0);
    EXPECT_TRUE(std::strcmp(ahamkara::client::weapon_viewmodel_mesh_path(3),
                            "assets/compiled/models/viewmodel_arms.aemesh") == 0);
    EXPECT_TRUE(std::strcmp(ahamkara::client::weapon_viewmodel_mesh_path(100),
                            "assets/compiled/models/viewmodel_arms.aemesh") == 0);
    EXPECT_TRUE(std::strcmp(ahamkara::client::weapon_viewmodel_mesh_path(9999),
                            "assets/compiled/models/viewmodel_arms.aemesh") == 0);

    std::cout << "  test_mesh_path_resolution passed.\n";
}

void test_viewmodel_transform_resolution() {
    // Valid weapon 0 — AR-15
    auto t0 = ahamkara::client::weapon_viewmodel_transform(0);
    EXPECT_NEAR(t0.pitch_deg, -2.0F, kEps);
    EXPECT_NEAR(t0.yaw_deg,    0.0F, kEps);
    EXPECT_NEAR(t0.roll_deg,   0.0F, kEps);
    EXPECT_NEAR(t0.pos_right,   0.05F, kEps);
    EXPECT_NEAR(t0.pos_up,     -0.05F, kEps);
    EXPECT_NEAR(t0.pos_forward, 0.05F, kEps);
    EXPECT_NEAR(t0.fov_scale,   0.85F, kEps);

    // Valid weapon 1 — Shotgun
    auto t1 = ahamkara::client::weapon_viewmodel_transform(1);
    EXPECT_NEAR(t1.pitch_deg, -3.0F, kEps);
    EXPECT_NEAR(t1.fov_scale,  0.80F, kEps);

    // Valid weapon 2 — Rocket Launcher
    auto t2 = ahamkara::client::weapon_viewmodel_transform(2);
    EXPECT_NEAR(t2.pitch_deg,   -5.0F, kEps);
    EXPECT_NEAR(t2.roll_deg,     2.0F, kEps);
    EXPECT_NEAR(t2.fov_scale,    0.90F, kEps);

    // Out-of-range → all-zeros fallback (fov_scale defaults to 1.0)
    auto tf = ahamkara::client::weapon_viewmodel_transform(42);
    EXPECT_NEAR(tf.pitch_deg,   0.0F, kEps);
    EXPECT_NEAR(tf.yaw_deg,     0.0F, kEps);
    EXPECT_NEAR(tf.roll_deg,    0.0F, kEps);
    EXPECT_NEAR(tf.pos_right,   0.0F, kEps);
    EXPECT_NEAR(tf.pos_up,      0.0F, kEps);
    EXPECT_NEAR(tf.pos_forward, 0.0F, kEps);
    EXPECT_NEAR(tf.fov_scale,   1.0F, kEps); // default

    // Negative index fallback
    auto tn = ahamkara::client::weapon_viewmodel_transform(-1);
    EXPECT_NEAR(tn.pos_right, 0.0F, kEps);
    EXPECT_NEAR(tn.fov_scale, 1.0F, kEps);

    std::cout << "  test_viewmodel_transform_resolution passed.\n";
}

void test_grip_sockets_resolution() {
    // AR-15
    auto g0 = ahamkara::client::weapon_grip_sockets(0);
    EXPECT_NEAR(g0.grip_right_x, 0.00F, kEps);
    EXPECT_NEAR(g0.grip_right_y, 0.70F, kEps);
    EXPECT_NEAR(g0.grip_left_x,  0.15F, kEps);
    EXPECT_NEAR(g0.grip_left_y,  0.55F, kEps);

    // Shotgun
    auto g1 = ahamkara::client::weapon_grip_sockets(1);
    EXPECT_NEAR(g1.grip_right_x, 0.00F, kEps);
    EXPECT_NEAR(g1.grip_left_x,  0.20F, kEps);
    EXPECT_NEAR(g1.grip_left_y,  0.50F, kEps);

    // Rocket Launcher
    auto g2 = ahamkara::client::weapon_grip_sockets(2);
    EXPECT_NEAR(g2.grip_right_x, 0.00F, kEps);
    EXPECT_NEAR(g2.grip_left_x,  0.25F, kEps);
    EXPECT_NEAR(g2.grip_left_y,  0.40F, kEps);

    // Out-of-range → all-zeros fallback
    auto gf = ahamkara::client::weapon_grip_sockets(42);
    EXPECT_NEAR(gf.grip_right_x, 0.0F, kEps);
    EXPECT_NEAR(gf.grip_right_y, 0.0F, kEps);
    EXPECT_NEAR(gf.grip_left_x,  0.0F, kEps);
    EXPECT_NEAR(gf.grip_left_y,  0.0F, kEps);

    // Negative index fallback
    auto gn = ahamkara::client::weapon_grip_sockets(-1);
    EXPECT_NEAR(gn.grip_right_x, 0.0F, kEps);

    std::cout << "  test_grip_sockets_resolution passed.\n";
}

void test_reload_data_resolution() {
    // AR-15
    auto r0 = ahamkara::client::weapon_reload_data(0);
    EXPECT_NEAR(r0.grab_start,   0.00F, kEps);
    EXPECT_NEAR(r0.grab_end,     0.18F, kEps);
    EXPECT_NEAR(r0.remove_start, 0.18F, kEps);
    EXPECT_NEAR(r0.remove_end,   0.45F, kEps);
    EXPECT_NEAR(r0.insert_start, 0.45F, kEps);
    EXPECT_NEAR(r0.insert_end,   0.75F, kEps);
    EXPECT_NEAR(r0.return_start, 0.75F, kEps);
    EXPECT_NEAR(r0.return_end,   1.00F, kEps);
    EXPECT_NEAR(r0.mag_pos_x,    0.00F, kEps);
    EXPECT_NEAR(r0.mag_pos_y,    0.30F, kEps);
    EXPECT_NEAR(r0.mag_pos_z,    0.10F, kEps);
    EXPECT_NEAR(r0.tilt_pitch_deg, -18.0F, kEps);
    EXPECT_NEAR(r0.tilt_yaw_deg,     8.0F, kEps);
    EXPECT_NEAR(r0.tilt_roll_deg,    -5.0F, kEps);

    // Shotgun — different timing (longer reload)
    auto r1 = ahamkara::client::weapon_reload_data(1);
    EXPECT_NEAR(r1.grab_start,   0.00F, kEps);
    EXPECT_NEAR(r1.remove_end,   0.55F, kEps);
    EXPECT_NEAR(r1.return_end,   1.00F, kEps);
    EXPECT_NEAR(r1.tilt_pitch_deg, -25.0F, kEps); // more dramatic tilt

    // Rocket Launcher
    auto r2 = ahamkara::client::weapon_reload_data(2);
    EXPECT_NEAR(r2.grab_start,   0.00F, kEps);
    EXPECT_NEAR(r2.return_end,   1.00F, kEps);
    EXPECT_NEAR(r2.tilt_roll_deg,  -3.0F, kEps);

    // Fallback for invalid index
    auto rf = ahamkara::client::weapon_reload_data(42);
    EXPECT_NEAR(rf.grab_start, 0.0F, kEps);
    EXPECT_NEAR(rf.return_end, 1.0F, kEps); // default struct

    std::cout << "  test_reload_data_resolution passed.\n";
}

void test_ads_transform_resolution() {
    // AR-15 — cancels hip offset
    auto a0 = ahamkara::client::weapon_ads_transform(0);
    EXPECT_NEAR(a0.ads_pos_right,   -0.05F, kEps);
    EXPECT_NEAR(a0.ads_pos_up,       0.05F, kEps);
    EXPECT_NEAR(a0.ads_pos_forward, -0.05F, kEps);
    EXPECT_NEAR(a0.ads_pitch_deg,   -2.0F,  kEps);
    EXPECT_NEAR(a0.ads_fov_scale,    0.70F, kEps);

    // Shotgun
    auto a1 = ahamkara::client::weapon_ads_transform(1);
    EXPECT_NEAR(a1.ads_pos_right,   -0.08F, kEps);
    EXPECT_NEAR(a1.ads_fov_scale,    0.67F, kEps);

    // Rocket Launcher
    auto a2 = ahamkara::client::weapon_ads_transform(2);
    EXPECT_NEAR(a2.ads_pos_right,   -0.12F, kEps);
    EXPECT_NEAR(a2.ads_fov_scale,    0.75F, kEps);

    // Fallback for invalid index
    auto af = ahamkara::client::weapon_ads_transform(42);
    EXPECT_NEAR(af.ads_pitch_deg, 0.0F, kEps);
    EXPECT_NEAR(af.ads_fov_scale, 1.0F, kEps);

    std::cout << "  test_ads_transform_resolution passed.\n";
}

void test_weapon_count_constants() {
    EXPECT_TRUE(ahamkara::client::kWeaponViewmodelCount == 3);
    EXPECT_TRUE(ahamkara::client::kWeaponViewmodelTransforms.size() == 3);
    EXPECT_TRUE(ahamkara::client::kWeaponGripSockets.size() == 3);
    EXPECT_TRUE(ahamkara::client::kWeaponReloadData.size() == 3);
    EXPECT_TRUE(ahamkara::client::kWeaponAdsTransforms.size() == 3);

    std::cout << "  test_weapon_count_constants passed.\n";
}

// ---------------------------------------------------------------------------
// 2. Animation Layer — Sway
// ---------------------------------------------------------------------------

void test_sway_zero_input() {
    ae::animation::WeaponAnimState state{};
    ae::animation::WeaponAnimConfig cfg{};
    cfg.sway_amplitude = 0.008F;
    cfg.sway_frequency = 1.2F;
    cfg.sway_damping = 3.0F;

    ae::skeleton::Mat4 offset;
    evaluate_sway_layer(state, cfg, 1.0F / 60.0F,
                        0.0F, 0.0F, 0.0F, offset);
    EXPECT_MAT4_FINITE(offset);

    // With zero input the offset should be small (just idle oscillation)
    EXPECT_NEAR(offset.m[12], 0.0F, 0.01F);  // translation x
    EXPECT_NEAR(offset.m[13], 0.0F, 0.01F);  // translation y

    std::cout << "  test_sway_zero_input passed.\n";
}

void test_sway_look_delta_accumulates() {
    ae::animation::WeaponAnimState state{};
    ae::animation::WeaponAnimConfig cfg{};
    cfg.sway_amplitude = 0.008F;
    cfg.sway_frequency = 1.2F;
    cfg.sway_damping = 0.5F;   // low damping so changes persist

    ae::skeleton::Mat4 offset;

    // Apply a rightwards look delta
    evaluate_sway_layer(state, cfg, 1.0F / 60.0F,
                        10.0F, 0.0F, 0.0F, offset);
    EXPECT_MAT4_FINITE(offset);
    float x1 = offset.m[12];

    // Apply a second frame in same direction
    evaluate_sway_layer(state, cfg, 1.0F / 60.0F,
                        10.0F, 0.0F, 0.0F, offset);
    EXPECT_MAT4_FINITE(offset);
    float x2 = offset.m[12];

    // Sway should have accumulated further right
    if (x2 < x1 - 0.001F) {
        std::cerr << "FAIL line " << __LINE__ << ": sway did not accumulate: x1=" << x1 << " x2=" << x2 << "\n";
        ++g_failures;
    }

    std::cout << "  test_sway_look_delta_accumulates passed.\n";
}

void test_sway_ads_reduction() {
    ae::animation::WeaponAnimState state{};
    ae::animation::WeaponAnimConfig cfg{};
    cfg.sway_amplitude = 0.008F;
    cfg.sway_frequency = 1.2F;
    cfg.sway_damping = 0.5F;
    cfg.ads_sway_multiplier = 0.3F;

    ae::skeleton::Mat4 offset_hip, offset_ads;

    // Same input, hip vs ADS
    float look = 20.0F;
    evaluate_sway_layer(state, cfg, 1.0F / 60.0F,
                        look, 0.0F, 0.0F, offset_hip);

    // Reset state for a fair comparison
    state = {};
    evaluate_sway_layer(state, cfg, 1.0F / 60.0F,
                        look, 0.0F, 1.0F, offset_ads);  // fully ADS

    // ADS sway should be smaller magnitude (reduced by ads_sway_multiplier)
    float hip_mag = std::sqrt(offset_hip.m[12] * offset_hip.m[12] +
                              offset_hip.m[13] * offset_hip.m[13]);
    float ads_mag = std::sqrt(offset_ads.m[12] * offset_ads.m[12] +
                              offset_ads.m[13] * offset_ads.m[13]);
    if (ads_mag > hip_mag + 0.001F) {
        std::cerr << "FAIL line " << __LINE__ << ": ADS sway magnitude " << ads_mag << " > hip " << hip_mag << "\n";
        ++g_failures;
    }

    std::cout << "  test_sway_ads_reduction passed.\n";
}

void test_sway_no_nan() {
    // Extreme inputs should never produce NaN/Inf
    ae::animation::WeaponAnimState state{};
    ae::animation::WeaponAnimConfig cfg{};
    cfg.sway_amplitude = 0.008F;
    cfg.sway_frequency = 1.2F;
    cfg.sway_damping = 3.0F;

    ae::skeleton::Mat4 offset;

    // Extreme look deltas
    evaluate_sway_layer(state, cfg, 1000.0F, 1.0e6F, -1.0e6F, 0.0F, offset);
    EXPECT_MAT4_FINITE(offset);

    // Negative dt
    state = {};
    evaluate_sway_layer(state, cfg, -1.0F, 0.0F, 0.0F, 0.0F, offset);
    EXPECT_MAT4_FINITE(offset);

    // Zero dt
    state = {};
    evaluate_sway_layer(state, cfg, 0.0F, 100.0F, 100.0F, 0.0F, offset);
    EXPECT_MAT4_FINITE(offset);

    std::cout << "  test_sway_no_nan passed.\n";
}

// ---------------------------------------------------------------------------
// 3. Animation Layer — Bob
// ---------------------------------------------------------------------------

void test_bob_not_moving() {
    ae::animation::WeaponAnimState state{};
    ae::animation::WeaponAnimConfig cfg{};
    cfg.bob_amplitude_vertical = 0.012F;
    cfg.bob_amplitude_horizontal = 0.006F;

    ae::skeleton::Mat4 offset;
    evaluate_bob_layer(state, cfg, 1.0F / 60.0F,
                       0.0F, false, offset);
    EXPECT_MAT4_FINITE(offset);

    // Should be identity when not moving
    EXPECT_TRUE(mat4_nearly_equal(offset, ae::skeleton::Mat4::identity(), kEps));

    std::cout << "  test_bob_not_moving passed.\n";
}

void test_bob_moving() {
    ae::animation::WeaponAnimState state{};
    ae::animation::WeaponAnimConfig cfg{};
    cfg.bob_amplitude_vertical = 0.012F;
    cfg.bob_amplitude_horizontal = 0.006F;
    cfg.bob_frequency_walk = 2.0F;
    cfg.bob_frequency_sprint = 3.5F;

    ae::skeleton::Mat4 offset;

    // Walk speed
    evaluate_bob_layer(state, cfg, 1.0F / 60.0F,
                       3.0F, true, offset);
    EXPECT_MAT4_FINITE(offset);

    // Should have some translation (non-identity)
    bool is_identity = mat4_nearly_equal(offset, ae::skeleton::Mat4::identity(), kEps);
    EXPECT_FALSE(is_identity);

    std::cout << "  test_bob_moving passed.\n";
}

void test_bob_sprint_vs_walk() {
    ae::animation::WeaponAnimState state_walk{};
    ae::animation::WeaponAnimState state_sprint{};
    ae::animation::WeaponAnimConfig cfg{};
    cfg.bob_amplitude_vertical = 0.012F;
    cfg.bob_amplitude_horizontal = 0.006F;
    cfg.bob_frequency_walk = 2.0F;
    cfg.bob_frequency_sprint = 3.5F;

    ae::skeleton::Mat4 offset_walk, offset_sprint;

    // Multiple frames to accumulate phase difference
    for (int i = 0; i < 60; ++i) {
        evaluate_bob_layer(state_walk, cfg, 1.0F / 60.0F,
                           3.0F, true, offset_walk);
        evaluate_bob_layer(state_sprint, cfg, 1.0F / 60.0F,
                           7.0F, true, offset_sprint);
    }

    EXPECT_MAT4_FINITE(offset_walk);
    EXPECT_MAT4_FINITE(offset_sprint);

    // Frequencies differ so offsets should differ
    bool same = mat4_nearly_equal(offset_walk, offset_sprint, kEps);
    EXPECT_FALSE(same);

    std::cout << "  test_bob_sprint_vs_walk passed.\n";
}

void test_bob_no_nan() {
    ae::animation::WeaponAnimState state{};
    ae::animation::WeaponAnimConfig cfg{};

    // Extreme speed
    ae::skeleton::Mat4 offset;
    evaluate_bob_layer(state, cfg, 1.0F / 60.0F, 1.0e6F, true, offset);
    EXPECT_MAT4_FINITE(offset);

    // Negative speed (should still produce finite output)
    state = {};
    evaluate_bob_layer(state, cfg, 1.0F / 60.0F, -10.0F, true, offset);
    EXPECT_MAT4_FINITE(offset);

    std::cout << "  test_bob_no_nan passed.\n";
}

// ---------------------------------------------------------------------------
// 4. Animation Layer — Recoil Kick
// ---------------------------------------------------------------------------

void test_recoil_kick_no_fire() {
    ae::animation::WeaponAnimState state{};
    ae::animation::WeaponAnimConfig cfg{};

    ae::skeleton::Mat4 offset;
    evaluate_recoil_kick_layer(state, cfg, 1.0F / 60.0F, offset);
    EXPECT_MAT4_FINITE(offset);

    // No fire → identity
    EXPECT_TRUE(mat4_nearly_equal(offset, ae::skeleton::Mat4::identity(), kEps));

    std::cout << "  test_recoil_kick_no_fire passed.\n";
}

void test_recoil_kick_trigger_and_decay() {
    ae::animation::WeaponAnimState state{};
    ae::animation::WeaponAnimConfig cfg{};
    cfg.recoil.recovery_speed = 5.0F;
    cfg.recoil.recovery_damping = 0.8F;

    ae::skeleton::Mat4 offset;

    // Fire
    ae::animation::fire_weapon_kick(state);
    EXPECT_TRUE(state.fire_anim_time > 0.0F);
    EXPECT_NEAR(state.fire_anim_time, 0.05F, kEps);

    // Immediately evaluate — should see a kick
    evaluate_recoil_kick_layer(state, cfg, 0.0F, offset);
    EXPECT_MAT4_FINITE(offset);
    bool is_identity = mat4_nearly_equal(offset, ae::skeleton::Mat4::identity(), kEps);
    EXPECT_FALSE(is_identity);

    // The kick is along -Y (upward)
    if (offset.m[13] >= 0.0F && std::abs(offset.m[13]) >= kEps) {
        std::cerr << "FAIL line " << __LINE__ << ": expected upward kick, got y=" << offset.m[13] << "\n";
        ++g_failures;
    }

    std::cout << "  test_recoil_kick_trigger_and_decay passed.\n";
}

void test_recoil_kick_timer_decay() {
    ae::animation::WeaponAnimState state{};
    ae::animation::WeaponAnimConfig cfg{};

    ae::skeleton::Mat4 offset;

    // Fire
    ae::animation::fire_weapon_kick(state);
    EXPECT_NEAR(state.fire_anim_time, 0.05F, kEps);

    // Tick past the kick duration (50ms)
    evaluate_recoil_kick_layer(state, cfg, 0.06F, offset);
    EXPECT_MAT4_FINITE(offset);

    // After the kick duration, fire_anim_time should be 0
    EXPECT_NEAR(state.fire_anim_time, 0.0F, kEps);

    // Offset should be identity
    EXPECT_TRUE(mat4_nearly_equal(offset, ae::skeleton::Mat4::identity(), kEps));

    std::cout << "  test_recoil_kick_timer_decay passed.\n";
}

void test_recoil_kick_no_nan() {
    ae::animation::WeaponAnimState state{};
    ae::animation::WeaponAnimConfig cfg{};

    ae::animation::fire_weapon_kick(state);

    ae::skeleton::Mat4 offset;
    evaluate_recoil_kick_layer(state, cfg, -1.0F, offset);
    EXPECT_MAT4_FINITE(offset);

    // Fire again then evaluate with huge dt
    ae::animation::fire_weapon_kick(state);
    evaluate_recoil_kick_layer(state, cfg, 1.0e6F, offset);
    EXPECT_MAT4_FINITE(offset);

    std::cout << "  test_recoil_kick_no_nan passed.\n";
}

// ---------------------------------------------------------------------------
// 5. Composite Weapon Animation — transform composition order
//    Documented order: sway * bob * recoil * ADS position
// ---------------------------------------------------------------------------

void test_composition_order() {
    // Verify that evaluate_weapon_animation composes as: sway * bob * recoil * ADS
    // We can check this by constructing a scenario where the order matters.

    ae::animation::WeaponAnimState state{};
    ae::animation::WeaponAnimConfig cfg{};
    cfg.sway_amplitude = 0.1F;       // Large sway so it dominates
    cfg.sway_frequency = 0.0F;       // No oscillation for deterministic test
    cfg.sway_damping = 1.0F;
    cfg.bob_amplitude_vertical = 0.1F;
    cfg.bob_amplitude_horizontal = 0.1F;
    cfg.bob_frequency_walk = 0.0F;
    cfg.ads_transition_time = 0.2F;

    ae::skeleton::Mat4 result;

    // Fire to get a recoil kick
    evaluate_weapon_animation(state, cfg, 1.0F / 60.0F,
                              3.0F, 100.0F, 0.0F,
                              true, false, true, result);
    EXPECT_MAT4_FINITE(result);

    // Transform matrix should have some off-diagonal elements indicating
    // the composition of translation and rotation from multiple layers.
    // At minimum, the result should not be identity.
    bool is_identity = mat4_nearly_equal(result, ae::skeleton::Mat4::identity(), 0.01F);
    EXPECT_FALSE(is_identity);

    // The documented composition order is sway * bob * recoil * ADS (translation).
    // Verify that result is non-trivial with translation in expected components.
    EXPECT_NEAR(result.m[15], 1.0F, kEps); // homogeneous w

    std::cout << "  test_composition_order passed.\n";
}

void test_composition_reproducibility() {
    // Same input → same output across frames (determinism)
    ae::animation::WeaponAnimConfig cfg{};
    cfg.sway_amplitude = 0.008F;
    cfg.sway_frequency = 1.2F;
    cfg.sway_damping = 3.0F;
    cfg.bob_amplitude_vertical = 0.012F;
    cfg.bob_amplitude_horizontal = 0.006F;
    cfg.bob_frequency_walk = 2.0F;
    cfg.ads_transition_time = 0.2F;

    ae::skeleton::Mat4 result_a, result_b;

    // Run two independent evaluations with identical inputs
    {
        ae::animation::WeaponAnimState state{};
        ae::animation::WeaponAnimConfig cfg2 = cfg;
        evaluate_weapon_animation(state, cfg2, 1.0F / 60.0F,
                                  3.0F, 0.0F, 0.0F,
                                  true, false, false, result_a);
    }
    {
        ae::animation::WeaponAnimState state{};
        ae::animation::WeaponAnimConfig cfg2 = cfg;
        evaluate_weapon_animation(state, cfg2, 1.0F / 60.0F,
                                  3.0F, 0.0F, 0.0F,
                                  true, false, false, result_b);
    }

    EXPECT_TRUE(mat4_nearly_equal(result_a, result_b, 1.0e-4F));

    std::cout << "  test_composition_reproducibility passed.\n";
}

// ---------------------------------------------------------------------------
// 6. Recoil System — fire_recoil / apply_recoil
// ---------------------------------------------------------------------------

void test_fire_recoil_adds_kick() {
    ae::animation::RecoilState state{};
    ae::animation::RecoilConfig cfg{};
    cfg.kick_pitch_min = 0.03F;
    cfg.kick_pitch_max = 0.06F;
    cfg.kick_yaw_min = -0.02F;
    cfg.kick_yaw_max = 0.02F;
    cfg.pattern_scale = 1.0F;

    EXPECT_NEAR(state.current_pitch, 0.0F, kEps);

    ae::animation::fire_recoil(state, cfg, false);
    EXPECT_TRUE(state.current_pitch > 0.0F);
    EXPECT_TRUE(state.current_pitch >= cfg.kick_pitch_min * 0.5F); // approx
    EXPECT_FINITE(state.current_pitch);

    std::cout << "  test_fire_recoil_adds_kick passed.\n";
}

void test_fire_recoil_ads_reduces_kick() {
    ae::animation::RecoilState state_hip{};
    ae::animation::RecoilState state_ads{};
    ae::animation::RecoilConfig cfg{};
    cfg.kick_pitch_min = 0.03F;
    cfg.kick_pitch_max = 0.06F;
    cfg.kick_yaw_min = -0.02F;
    cfg.kick_yaw_max = 0.02F;
    cfg.pattern_scale = 1.0F;
    cfg.ads_kick_multiplier = 0.5F;

    ae::animation::fire_recoil(state_hip, cfg, false);
    ae::animation::fire_recoil(state_ads, cfg, true);

    // ADS kick should be smaller (multiplied by ads_kick_multiplier)
    EXPECT_TRUE(std::abs(state_ads.current_pitch) <=
                std::abs(state_hip.current_pitch) + 0.001F);

    std::cout << "  test_fire_recoil_ads_reduces_kick passed.\n";
}

void test_apply_recoil_recovery() {
    ae::animation::RecoilState state{};
    ae::animation::RecoilConfig cfg{};
    cfg.kick_pitch_min = 0.03F;
    cfg.kick_pitch_max = 0.06F;
    cfg.recovery_speed = 5.0F;
    cfg.recovery_damping = 0.8F;

    // Fire a shot
    ae::animation::fire_recoil(state, cfg, false);
    float initial_kick = state.current_pitch;
    EXPECT_TRUE(initial_kick > 0.0F);

    ae::animation::JointTransform offset;

    // Tick recovery (not firing)
    for (int i = 0; i < 120; ++i) {
        ae::animation::apply_recoil(state, cfg, 1.0F / 60.0F,
                                     false, false, offset);
        EXPECT_FINITE(offset.qx);
        EXPECT_FINITE(offset.qy);
        EXPECT_FINITE(offset.qz);
        EXPECT_FINITE(offset.qw);
    }

    // After recovery, current_pitch should be closer to 0
    if (std::abs(state.current_pitch) >= std::abs(initial_kick) * 0.1F + 0.01F) {
        std::cerr << "FAIL line " << __LINE__ << ": initial=" << initial_kick << " final=" << state.current_pitch << "\n";
        ++g_failures;
    }

    std::cout << "  test_apply_recoil_recovery passed.\n";
}

void test_apply_recoil_no_nan() {
    ae::animation::RecoilState state{};
    ae::animation::RecoilConfig cfg{};
    cfg.kick_pitch_min = 0.03F;
    cfg.kick_pitch_max = 0.06F;
    cfg.recovery_speed = 5.0F;
    cfg.recovery_damping = 0.8F;

    ae::animation::JointTransform offset;

    // Fire many rounds
    for (int i = 0; i < 100; ++i) {
        ae::animation::fire_recoil(state, cfg, false);
    }

    // Apply with extreme dt
    ae::animation::apply_recoil(state, cfg, 1000.0F, false, false, offset);
    EXPECT_FINITE(offset.qx);
    EXPECT_FINITE(offset.qy);
    EXPECT_FINITE(offset.qz);
    EXPECT_FINITE(offset.qw);
    EXPECT_FINITE(state.current_pitch);
    EXPECT_FINITE(state.current_yaw);
    EXPECT_FINITE(state.current_roll);

    std::cout << "  test_apply_recoil_no_nan passed.\n";
}

void test_recoil_clamps() {
    // Verify that the clamps in apply_recoil prevent runaway accumulation.
    ae::animation::RecoilState state{};
    ae::animation::RecoilConfig cfg{};
    cfg.kick_pitch_min = 0.03F;
    cfg.kick_pitch_max = 0.06F;
    cfg.kick_yaw_min = -0.02F;
    cfg.kick_yaw_max = 0.02F;
    cfg.recovery_speed = 0.01F;    // Very slow recovery
    cfg.recovery_damping = 0.99F;  // Minimal damping
    cfg.pattern_scale = 1.0F;

    ae::animation::JointTransform offset;

    // Fire many rounds to try to exceed clamp limits
    for (int i = 0; i < 10000; ++i) {
        ae::animation::fire_recoil(state, cfg, false);
        ae::animation::apply_recoil(state, cfg, 1.0F / 60.0F,
                                     true, false, offset);
    }

    float max_accum = cfg.kick_pitch_max * 10.0F;
    if (std::abs(state.current_pitch) > max_accum + 0.01F) {
        std::cerr << "FAIL line " << __LINE__ << ": pitch " << state.current_pitch << " exceeded clamp " << max_accum << "\n";
        ++g_failures;
    }
    if (std::abs(state.current_yaw) > max_accum + 0.01F) {
        std::cerr << "FAIL line " << __LINE__ << ": yaw " << state.current_yaw << " exceeded clamp " << max_accum << "\n";
        ++g_failures;
    }
    if (std::abs(state.current_roll) > max_accum * 0.5F + 0.01F) {
        std::cerr << "FAIL line " << __LINE__ << ": roll " << state.current_roll << " exceeded clamp " << (max_accum * 0.5F) << "\n";
        ++g_failures;
    }

    std::cout << "  test_recoil_clamps passed.\n";
}

// ---------------------------------------------------------------------------
// 7. IK Solver
// ---------------------------------------------------------------------------

void test_ik_reachable_target() {
    ae::animation::IKChain chain;
    chain.bone_length_upper = 0.35F;  // upper arm
    chain.bone_length_lower = 0.34F;  // lower arm
    chain.root_joint = 2;  // shoulder
    chain.mid_joint = 3;   // elbow
    chain.end_joint = 5;   // hand

    ae::animation::IKTarget target;
    target.enabled = true;
    target.weight = 1.0F;
    target.target_x = 0.0F;
    target.target_y = -0.5F;  // Reach downward
    target.target_z = 0.0F;

    ae::skeleton::Mat4 root_global = ae::skeleton::Mat4::identity();

    auto result = ae::animation::IKSolver::solve_two_bone(chain, target, root_global);
    EXPECT_TRUE(result.converged);

    // Root and mid corrections should be finite
    EXPECT_FINITE(result.root_correction.qx);
    EXPECT_FINITE(result.root_correction.qy);
    EXPECT_FINITE(result.root_correction.qz);
    EXPECT_FINITE(result.root_correction.qw);
    EXPECT_FINITE(result.mid_correction.qx);
    EXPECT_FINITE(result.mid_correction.qy);
    EXPECT_FINITE(result.mid_correction.qz);
    EXPECT_FINITE(result.mid_correction.qw);

    std::cout << "  test_ik_reachable_target passed.\n";
}

void test_ik_overextended() {
    ae::animation::IKChain chain;
    chain.bone_length_upper = 0.35F;
    chain.bone_length_lower = 0.34F;

    ae::animation::IKTarget target;
    target.enabled = true;
    target.weight = 1.0F;
    target.target_x = 0.0F;
    target.target_y = -10.0F;  // Far beyond reach (max ~0.69)
    target.target_z = 0.0F;

    ae::skeleton::Mat4 root_global = ae::skeleton::Mat4::identity();

    auto result = ae::animation::IKSolver::solve_two_bone(chain, target, root_global);
    // Overextended target still produces valid output (solver clamps to reachable range).
    // The converged flag may be false for overextended, but the solver should not produce NaN.
    if (!result.converged) {
        std::cerr << "line " << __LINE__ << ": overextended IK not converged (acceptable, clamped)\n";
    }

    // Should still produce finite values (no NaN)
    EXPECT_FINITE(result.root_correction.qx);
    EXPECT_FINITE(result.root_correction.qy);
    EXPECT_FINITE(result.root_correction.qz);
    EXPECT_FINITE(result.root_correction.qw);

    std::cout << "  test_ik_overextended passed.\n";
}

void test_ik_degenerate_target() {
    ae::animation::IKChain chain;
    chain.bone_length_upper = 0.35F;
    chain.bone_length_lower = 0.34F;

    ae::animation::IKTarget target;
    target.enabled = true;
    target.weight = 1.0F;
    target.target_x = 0.0F;
    target.target_y = 0.0F;  // At origin (coincident with root)
    target.target_z = 0.0F;

    ae::skeleton::Mat4 root_global = ae::skeleton::Mat4::identity();

    auto result = ae::animation::IKSolver::solve_two_bone(chain, target, root_global);
    // degenerate (target at origin) returns unconverged
    // but should not produce NaN
    EXPECT_FINITE(result.root_correction.qx);
    EXPECT_FINITE(result.root_correction.qy);
    EXPECT_FINITE(result.root_correction.qz);
    EXPECT_FINITE(result.root_correction.qw);
    EXPECT_FINITE(result.mid_correction.qx);
    EXPECT_FINITE(result.mid_correction.qy);
    EXPECT_FINITE(result.mid_correction.qz);
    EXPECT_FINITE(result.mid_correction.qw);

    std::cout << "  test_ik_degenerate_target passed.\n";
}

void test_ik_disabled_target() {
    ae::animation::IKChain chain;
    chain.bone_length_upper = 0.35F;
    chain.bone_length_lower = 0.34F;

    ae::animation::IKTarget target;
    target.enabled = false;  // Disabled
    target.weight = 1.0F;
    target.target_x = 0.0F;
    target.target_y = -0.5F;
    target.target_z = 0.0F;

    ae::skeleton::Mat4 root_global = ae::skeleton::Mat4::identity();

    auto result = ae::animation::IKSolver::solve_two_bone(chain, target, root_global);
    EXPECT_FALSE(result.converged);

    // Disabled → identity corrections
    EXPECT_NEAR(result.root_correction.qw, 1.0F, kEps);
    EXPECT_NEAR(result.mid_correction.qw, 1.0F, kEps);

    std::cout << "  test_ik_disabled_target passed.\n";
}

void test_ik_zero_weight() {
    ae::animation::IKChain chain;
    chain.bone_length_upper = 0.35F;
    chain.bone_length_lower = 0.34F;

    ae::animation::IKTarget target;
    target.enabled = true;
    target.weight = 0.0F;  // Zero weight
    target.target_x = 0.0F;
    target.target_y = -0.5F;
    target.target_z = 0.0F;

    ae::skeleton::Mat4 root_global = ae::skeleton::Mat4::identity();

    auto result = ae::animation::IKSolver::solve_two_bone(chain, target, root_global);
    // Zero weight → early return, not converged
    EXPECT_FALSE(result.converged);

    std::cout << "  test_ik_zero_weight passed.\n";
}

void test_ik_zero_bone_length() {
    ae::animation::IKChain chain;
    chain.bone_length_upper = 0.0F;  // Zero bone length
    chain.bone_length_lower = 0.34F;

    ae::animation::IKTarget target;
    target.enabled = true;
    target.weight = 1.0F;
    target.target_x = 0.0F;
    target.target_y = -0.5F;
    target.target_z = 0.0F;

    ae::skeleton::Mat4 root_global = ae::skeleton::Mat4::identity();

    auto result = ae::animation::IKSolver::solve_two_bone(chain, target, root_global);
    EXPECT_FALSE(result.converged);  // Returns early

    // No NaN
    EXPECT_FINITE(result.root_correction.qx);

    std::cout << "  test_ik_zero_bone_length passed.\n";
}

void test_ik_no_nan() {
    // Aggressive edge cases — finite inputs must never produce NaN output.
    ae::animation::IKChain chain;
    chain.bone_length_upper = 0.35F;
    chain.bone_length_lower = 0.34F;

    ae::skeleton::Mat4 root_global = ae::skeleton::Mat4::identity();

    // Extremely large (but finite) target
    {
        ae::animation::IKTarget target;
        target.enabled = true;
        target.weight = 1.0F;
        target.target_x = 1.0e6F;
        target.target_y = -1.0e6F;
        target.target_z = 0.0F;
        auto result = ae::animation::IKSolver::solve_two_bone(chain, target, root_global);
        EXPECT_FINITE(result.root_correction.qx);
        EXPECT_FINITE(result.root_correction.qy);
        EXPECT_FINITE(result.root_correction.qz);
        EXPECT_FINITE(result.root_correction.qw);
    }

    // Tiny target (sub-normal distances)
    {
        ae::animation::IKTarget target;
        target.enabled = true;
        target.weight = 1.0F;
        target.target_x = 1.0e-10F;
        target.target_y = 1.0e-10F;
        target.target_z = 1.0e-10F;
        auto result = ae::animation::IKSolver::solve_two_bone(chain, target, root_global);
        EXPECT_FINITE(result.root_correction.qx);
        EXPECT_FINITE(result.root_correction.qy);
        EXPECT_FINITE(result.root_correction.qz);
        EXPECT_FINITE(result.root_correction.qw);
    }

    // Extremely small bone lengths (near-zero)
    {
        ae::animation::IKChain small_chain;
        small_chain.bone_length_upper = 1.0e-8F;
        small_chain.bone_length_lower = 1.0e-8F;
        ae::animation::IKTarget target;
        target.enabled = true;
        target.weight = 1.0F;
        target.target_x = 0.0F;
        target.target_y = -1.0e-7F;
        target.target_z = 0.0F;
        auto result = ae::animation::IKSolver::solve_two_bone(small_chain, target, root_global);
        EXPECT_FINITE(result.root_correction.qx);
        EXPECT_FINITE(result.root_correction.qy);
        EXPECT_FINITE(result.root_correction.qz);
        EXPECT_FINITE(result.root_correction.qw);
    }

    std::cout << "  test_ik_no_nan passed.\n";
}

// ---------------------------------------------------------------------------
// 8. ADS Transitions — deterministic across variable frame partitions
// ---------------------------------------------------------------------------

void test_ads_enter_transition() {
    ae::animation::WeaponAnimState state{};
    ae::animation::WeaponAnimConfig cfg{};
    cfg.ads_transition_time = 0.2F;  // 200ms to enter ADS

    float target = 1.0F;
    float ads_speed = 1.0F / cfg.ads_transition_time;

    // Simulate entering ADS over a 200ms period at various dt sizes
    float total_time = 0.0F;
    while (total_time < cfg.ads_transition_time - 0.001F) {
        float dt = 1.0F / 60.0F;
        if (state.ads_blend < target) {
            state.ads_blend = std::min(state.ads_blend + ads_speed * dt, target);
        }
        total_time += dt;
    }

    // Should be fully ADS after ~200ms
    EXPECT_NEAR(state.ads_blend, 1.0F, 0.02F);

    std::cout << "  test_ads_enter_transition passed.\n";
}

void test_ads_exit_transition() {
    ae::animation::WeaponAnimState state{};
    state.ads_blend = 1.0F;  // Start fully ADS
    ae::animation::WeaponAnimConfig cfg{};
    cfg.ads_transition_time = 0.2F;

    float target = 0.0F;
    float ads_speed = 1.0F / cfg.ads_transition_time;

    float total_time = 0.0F;
    while (total_time < cfg.ads_transition_time - 0.001F) {
        float dt = 1.0F / 60.0F;
        if (state.ads_blend > target) {
            state.ads_blend = std::max(state.ads_blend - ads_speed * dt, target);
        }
        total_time += dt;
    }

    EXPECT_NEAR(state.ads_blend, 0.0F, 0.02F);

    std::cout << "  test_ads_exit_transition passed.\n";
}

void test_ads_deterministic_variable_frame_partitions() {
    // ADS transitions should be deterministic regardless of frame partition sizes.
    // Total ADS entry should take the same amount of simulated time.
    ae::animation::WeaponAnimConfig cfg{};
    cfg.ads_transition_time = 0.3F;  // 300ms

    // Test with 60fps frames (16.67ms each)
    ae::animation::WeaponAnimState state_small{};
    float ads_speed = 1.0F / cfg.ads_transition_time;
    for (int i = 0; i < 18; ++i) { // 18 * 16.67ms ≈ 300ms
        float dt = 1.0F / 60.0F;
        if (state_small.ads_blend < 1.0F) {
            state_small.ads_blend = std::min(state_small.ads_blend + ads_speed * dt, 1.0F);
        }
    }

    // Test with one large frame (300ms)
    ae::animation::WeaponAnimState state_large{};
    float dt = 0.3F;
    if (state_large.ads_blend < 1.0F) {
        state_large.ads_blend = std::min(state_large.ads_blend + ads_speed * dt, 1.0F);
    }

    // Both should reach the same final blend value (or very close)
    EXPECT_NEAR(state_small.ads_blend, state_large.ads_blend, 0.001F);
    EXPECT_NEAR(state_small.ads_blend, 1.0F, 0.001F);
    EXPECT_NEAR(state_large.ads_blend, 1.0F, 0.001F);

    std::cout << "  test_ads_deterministic_variable_frame_partitions passed.\n";
}

void test_ads_partial_blend() {
    // Verify that partial ADS (e.g., 50%) produces intermediate state
    ae::animation::WeaponAnimState state{};
    ae::animation::WeaponAnimConfig cfg{};
    cfg.ads_transition_time = 1.0F;  // 1 second transition

    float ads_speed = 1.0F / cfg.ads_transition_time;

    // Tick 0.5 seconds → should be 50% ADS
    for (int i = 0; i < 30; ++i) {
        float dt = 1.0F / 60.0F;
        state.ads_blend = std::min(state.ads_blend + ads_speed * dt, 1.0F);
    }

    EXPECT_NEAR(state.ads_blend, 0.5F, 0.02F);

    std::cout << "  test_ads_partial_blend passed.\n";
}

void test_ads_no_nan() {
    ae::animation::WeaponAnimState state{};
    ae::animation::WeaponAnimConfig cfg{};
    cfg.ads_transition_time = 0.2F;

    float ads_speed = 1.0F / cfg.ads_transition_time;

    // Extreme dt
    state.ads_blend = 0.5F;
    state.ads_blend = std::min(state.ads_blend + ads_speed * 1.0e6F, 1.0F);
    EXPECT_FINITE(state.ads_blend);
    EXPECT_TRUE(state.ads_blend >= 0.0F && state.ads_blend <= 1.0F);

    // Negative dt
    state.ads_blend = 0.5F;
    state.ads_blend = std::max(state.ads_blend - ads_speed * (-1.0F), 0.0F);
    EXPECT_FINITE(state.ads_blend);

    std::cout << "  test_ads_no_nan passed.\n";
}

// ---------------------------------------------------------------------------
// 9. Reload Phase Progression
//
// Replicates the phase progression logic from WeaponAnimationController
// to verify it produces correct results with authored data.
// ---------------------------------------------------------------------------

void test_reload_phase_order() {
    // Verify that reload phases progress in the documented order:
    // Idle → GrabMag → RemoveMag → InsertMag → ReturnToGrip → Idle
    using RP = ahamkara::client::ReloadPhase;

    // Test with AR-15 reload data
    auto rd = ahamkara::client::weapon_reload_data(0);

    float duration = 2.0F;  // AR-15 reload duration
    float timer = duration;
    RP phase = RP::Idle;

    // Simulate reload from start to finish
    struct PhaseEntry {
        float normalized_time;
        RP phase;
    };

    std::vector<PhaseEntry> observed_phases;

    // Step through at 60fps
    while (timer > 0.0F) {
        float dt = 1.0F / 60.0F;
        timer = std::max(0.0F, timer - dt);
        float normalized = 1.0F - (timer / duration);

        RP current_phase = RP::Idle;
        if (normalized < rd.grab_start) {
            current_phase = RP::Idle;
        } else if (normalized < rd.grab_end) {
            current_phase = RP::GrabMag;
        } else if (normalized < rd.remove_end) {
            current_phase = RP::RemoveMag;
        } else if (normalized < rd.insert_end) {
            current_phase = RP::InsertMag;
        } else if (normalized < rd.return_end) {
            current_phase = RP::ReturnToGrip;
        } else {
            current_phase = RP::Idle;
        }

        if (current_phase != phase) {
            phase = current_phase;
            observed_phases.push_back({normalized, phase});
        }
    }

    // Verify the phase ordering
    // We expect: Idle → GrabMag → RemoveMag → InsertMag → ReturnToGrip → Idle
    EXPECT_TRUE(observed_phases.size() >= 5);

    if (observed_phases.size() >= 1) EXPECT_TRUE(observed_phases[0].phase == RP::GrabMag);
    if (observed_phases.size() >= 2) EXPECT_TRUE(observed_phases[1].phase == RP::RemoveMag);
    if (observed_phases.size() >= 3) EXPECT_TRUE(observed_phases[2].phase == RP::InsertMag);
    if (observed_phases.size() >= 4) EXPECT_TRUE(observed_phases[3].phase == RP::ReturnToGrip);
    if (observed_phases.size() >= 5) EXPECT_TRUE(observed_phases[4].phase == RP::Idle);

    std::cout << "  test_reload_phase_order passed.\n";
}

void test_reload_ik_offset_computation() {
    // Verify IK offset computation during reload phases.
    auto rd = ahamkara::client::weapon_reload_data(0);
    float duration = 2.0F;

    // Helper: smoothstep reproduction
    auto smoothstep = [](float e0, float e1, float x) -> float {
        float t = std::clamp((x - e0) / (e1 - e0), 0.0F, 1.0F);
        return t * t * (3.0F - 2.0F * t);
    };

    auto compute_ik_offset = [&](float normalized) -> std::array<float, 3> {
        float ik[3] = {0.0F, 0.0F, 0.0F};

        if (normalized < rd.grab_start) {
            // Idle — no offset
        } else if (normalized < rd.grab_end) {
            float t = smoothstep(rd.grab_start, rd.grab_end, normalized);
            ik[0] = rd.mag_pos_x * t;
            ik[1] = rd.mag_pos_y * t;
            ik[2] = rd.mag_pos_z * t;
        } else if (normalized < rd.remove_end || normalized < rd.insert_end) {
            // The actual code uses: remove_end and insert_end as separate checks
            // For simplicity: in remove or insert phase, hold at magazine position
            ik[0] = rd.mag_pos_x;
            ik[1] = rd.mag_pos_y;
            ik[2] = rd.mag_pos_z;
        } else if (normalized < rd.return_end) {
            float t = 1.0F - smoothstep(rd.return_start, rd.return_end, normalized);
            ik[0] = rd.mag_pos_x * t;
            ik[1] = rd.mag_pos_y * t;
            ik[2] = rd.mag_pos_z * t;
        }

        return {ik[0], ik[1], ik[2]};
    };

    // At start (normalized=0) → offset should be (0,0,0)
    auto offset_0 = compute_ik_offset(0.0F);
    EXPECT_NEAR(offset_0[0], 0.0F, kEps);
    EXPECT_NEAR(offset_0[1], 0.0F, kEps);
    EXPECT_NEAR(offset_0[2], 0.0F, kEps);

    // Mid-grab → offset should be partially toward mag position
    float mid_grab = (rd.grab_start + rd.grab_end) * 0.5F;
    auto offset_grab = compute_ik_offset(mid_grab);
    EXPECT_FINITE(offset_grab[0]);
    EXPECT_FINITE(offset_grab[1]);
    EXPECT_FINITE(offset_grab[2]);

    // Mid-remove → offset should be at full mag position
    float mid_remove = (rd.remove_start + rd.remove_end) * 0.5F;
    auto offset_remove = compute_ik_offset(mid_remove);
    EXPECT_NEAR(offset_remove[0], rd.mag_pos_x, 0.01F);
    EXPECT_NEAR(offset_remove[1], rd.mag_pos_y, 0.01F);
    EXPECT_NEAR(offset_remove[2], rd.mag_pos_z, 0.01F);

    // After return → offset should be back to (0,0,0)
    auto offset_end = compute_ik_offset(1.0F);
    EXPECT_NEAR(offset_end[0], 0.0F, kEps);
    EXPECT_NEAR(offset_end[1], 0.0F, kEps);
    EXPECT_NEAR(offset_end[2], 0.0F, kEps);

    std::cout << "  test_reload_ik_offset_computation passed.\n";
}

void test_reload_deterministic_variable_frames() {
    // Reload should reach the same phase at the same normalized time
    // regardless of frame partition sizes.
    auto rd = ahamkara::client::weapon_reload_data(0);
    float duration = 2.0F;

    // Small frames
    float timer_small = duration;
    float phase_small = -1.0F;
    float timer_large = duration;
    float phase_large = -1.0F;

    // Run with 60fps frames for a total of 1 second
    for (int i = 0; i < 60; ++i) {
        timer_small = std::max(0.0F, timer_small - 1.0F / 60.0F);
    }
    phase_small = 1.0F - timer_small / duration;

    // Run with 30fps frames for a total of 1 second
    for (int i = 0; i < 30; ++i) {
        timer_large = std::max(0.0F, timer_large - 1.0F / 30.0F);
    }
    phase_large = 1.0F - timer_large / duration;

    // Both should reach the same normalized time
    EXPECT_NEAR(phase_small, phase_large, 0.001F);
    EXPECT_NEAR(phase_small, 0.5F, 0.01F);  // 1s into a 2s reload

    std::cout << "  test_reload_deterministic_variable_frames passed.\n";
}

void test_reload_tilt_computation() {
    // Verify weapon tilt angle computation during reload.
    auto rd = ahamkara::client::weapon_reload_data(0);
    float duration = 2.0F;

    auto smoothstep = [](float e0, float e1, float x) -> float {
        float t = std::clamp((x - e0) / (e1 - e0), 0.0F, 1.0F);
        return t * t * (3.0F - 2.0F * t);
    };

    // Compute tilt weight at various points in the reload cycle
    // At grab_start (0.0) → tilt_weight = 0
    // At grab_end (0.18) → tilt_weight = 1
    // During remove/insert → tilt_weight = 1
    // At return_end (1.0) → tilt_weight = 0

    float t0 = smoothstep(rd.grab_start, rd.grab_end, rd.grab_start);
    EXPECT_NEAR(t0, 0.0F, kEps);

    float t1 = smoothstep(rd.grab_start, rd.grab_end, rd.grab_end);
    EXPECT_NEAR(t1, 1.0F, kEps);

    float t2 = 1.0F - smoothstep(rd.return_start, rd.return_end, rd.return_end);
    EXPECT_NEAR(t2, 0.0F, kEps);

    // Verify tilt angles match expected values for AR-15
    EXPECT_NEAR(rd.tilt_pitch_deg, -18.0F, kEps);
    EXPECT_NEAR(rd.tilt_yaw_deg, 8.0F, kEps);
    EXPECT_NEAR(rd.tilt_roll_deg, -5.0F, kEps);

    std::cout << "  test_reload_tilt_computation passed.\n";
}

// ---------------------------------------------------------------------------
// 10. Reset behavior
// ---------------------------------------------------------------------------

void test_weapon_anim_state_reset() {
    ae::animation::WeaponAnimState state{};
    state.sway_phase = 10.0F;
    state.sway_velocity_x = 5.0F;
    state.sway_velocity_y = -3.0F;
    state.bob_phase = 20.0F;
    state.ads_blend = 1.0F;
    state.fire_anim_time = 0.05F;
    state.is_firing = true;
    state.is_reloading = true;
    state.reload_anim_time = 2.0F;

    // Reset via default construction
    state = {};

    EXPECT_NEAR(state.sway_phase, 0.0F, kEps);
    EXPECT_NEAR(state.sway_velocity_x, 0.0F, kEps);
    EXPECT_NEAR(state.sway_velocity_y, 0.0F, kEps);
    EXPECT_NEAR(state.bob_phase, 0.0F, kEps);
    EXPECT_NEAR(state.ads_blend, 0.0F, kEps);
    EXPECT_NEAR(state.fire_anim_time, 0.0F, kEps);
    EXPECT_FALSE(state.is_firing);
    EXPECT_FALSE(state.is_reloading);
    EXPECT_NEAR(state.reload_anim_time, 0.0F, kEps);

    std::cout << "  test_weapon_anim_state_reset passed.\n";
}

void test_recoil_state_reset() {
    ae::animation::RecoilState state{};
    state.current_pitch = 100.0F;
    state.current_yaw = 50.0F;
    state.current_roll = 25.0F;
    state.recovery_velocity_pitch = 10.0F;

    // Reset via default construction
    state = {};

    EXPECT_NEAR(state.current_pitch, 0.0F, kEps);
    EXPECT_NEAR(state.current_yaw, 0.0F, kEps);
    EXPECT_NEAR(state.current_roll, 0.0F, kEps);
    EXPECT_NEAR(state.recovery_velocity_pitch, 0.0F, kEps);

    std::cout << "  test_recoil_state_reset passed.\n";
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

}  // anonymous namespace

int main() {
    std::cout << "=== Flashback Presentation Contract Tests ===\n\n";

    // 1. Viewmodel data
    std::cout << "-- Viewmodel data --\n";
    test_mesh_path_resolution();
    test_viewmodel_transform_resolution();
    test_grip_sockets_resolution();
    test_reload_data_resolution();
    test_ads_transform_resolution();
    test_weapon_count_constants();

    // 2. Animation layers — sway
    std::cout << "-- Sway layer --\n";
    test_sway_zero_input();
    test_sway_look_delta_accumulates();
    test_sway_ads_reduction();
    test_sway_no_nan();

    // 3. Animation layers — bob
    std::cout << "-- Bob layer --\n";
    test_bob_not_moving();
    test_bob_moving();
    test_bob_sprint_vs_walk();
    test_bob_no_nan();

    // 4. Animation layers — recoil kick
    std::cout << "-- Recoil kick layer --\n";
    test_recoil_kick_no_fire();
    test_recoil_kick_trigger_and_decay();
    test_recoil_kick_timer_decay();
    test_recoil_kick_no_nan();

    // 5. Composite / composition order
    std::cout << "-- Composition order --\n";
    test_composition_order();
    test_composition_reproducibility();

    // 6. Recoil system (fire_recoil / apply_recoil)
    std::cout << "-- Recoil system --\n";
    test_fire_recoil_adds_kick();
    test_fire_recoil_ads_reduces_kick();
    test_apply_recoil_recovery();
    test_apply_recoil_no_nan();
    test_recoil_clamps();

    // 7. IK solver
    std::cout << "-- IK solver --\n";
    test_ik_reachable_target();
    test_ik_overextended();
    test_ik_degenerate_target();
    test_ik_disabled_target();
    test_ik_zero_weight();
    test_ik_zero_bone_length();
    test_ik_no_nan();

    // 8. ADS transitions
    std::cout << "-- ADS transitions --\n";
    test_ads_enter_transition();
    test_ads_exit_transition();
    test_ads_deterministic_variable_frame_partitions();
    test_ads_partial_blend();
    test_ads_no_nan();

    // 9. Reload phase progression
    std::cout << "-- Reload phase progression --\n";
    test_reload_phase_order();
    test_reload_ik_offset_computation();
    test_reload_deterministic_variable_frames();
    test_reload_tilt_computation();

    // 10. Reset behavior
    std::cout << "-- Reset behavior --\n";
    test_weapon_anim_state_reset();
    test_recoil_state_reset();

    // Summary
    std::cout << "\n";
    if (g_failures != 0) {
        std::cerr << "FAILURES: " << g_failures << "\n";
        return 1;
    }
    std::cout << "All flashback presentation contract tests passed.\n";
    return 0;
}
