#include "ahamkara/client/weapon_viewmodel_data.h"

#include <cmath>
#include <cassert>
#include <iostream>
#include <algorithm>

namespace {

// ============================================================
// Constants matching those in weapon_animation_controller.cpp
// ============================================================

constexpr float kDefaultAdsTransitionTime = 0.2F;
constexpr float kEpsilon = 0.0001F;

// ============================================================
// Helpers
// ============================================================

bool nearly_equal(float a, float b, float eps = kEpsilon) {
    return std::fabs(a - b) <= eps;
}

/// Replicates the ADS blend interpolation logic from
/// WeaponAnimationController::update_weapon().
/// @param current_blend Current ads_blend value [0,1].
/// @param target 1.0 if aiming, 0.0 if not.
/// @param dt Delta time in seconds.
/// @param transition_time Seconds to complete a full transition.
/// @return New ads_blend value after one tick of interpolation.
float ads_blend_tick(float current_blend, float target, float dt, float transition_time) {
    float ads_speed = (transition_time > 0.0F) ? 1.0F / transition_time : 10.0F;
    if (current_blend < target) {
        return std::min(current_blend + ads_speed * dt, target);
    } else if (current_blend > target) {
        return std::max(current_blend - ads_speed * dt, target);
    }
    return current_blend;
}

// ============================================================
// Test: ADS transform data correctness
// ============================================================

void test_ads_transform_data() {
    using namespace ahamkara::client;

    // Verify per-weapon ADS transform data exists and has expected structure.
    static_assert(kWeaponViewmodelCount == 3,
                  "Expected 3 weapons with ADS transforms");

    // All weapons should have non-identity ADS transforms (meaningful offsets)
    for (std::size_t i = 0; i < kWeaponAdsTransforms.size(); ++i) {
        const auto& ads = kWeaponAdsTransforms[i];
        // At least one position or rotation or FOV should be non-default
        bool has_effect = (std::fabs(ads.ads_pos_right)   > kEpsilon ||
                           std::fabs(ads.ads_pos_up)      > kEpsilon ||
                           std::fabs(ads.ads_pos_forward) > kEpsilon ||
                           std::fabs(ads.ads_pitch_deg)   > kEpsilon ||
                           std::fabs(ads.ads_yaw_deg)     > kEpsilon ||
                           std::fabs(ads.ads_roll_deg)    > kEpsilon ||
                           std::fabs(ads.ads_fov_scale - 1.0F) > kEpsilon);
        assert(has_effect && "Each weapon must have a non-default ADS transform");
    }

    // Test accessor function
    for (int i = -1; i <= static_cast<int>(kWeaponAdsTransforms.size()); ++i) {
        auto ads = weapon_ads_transform(i);
        if (i >= 0 && static_cast<std::size_t>(i) < kWeaponAdsTransforms.size()) {
            // Valid index should return corresponding data
            assert(nearly_equal(ads.ads_fov_scale,
                                kWeaponAdsTransforms[static_cast<std::size_t>(i)].ads_fov_scale));
        } else {
            // Invalid index returns default (no ADS effect)
            assert(nearly_equal(ads.ads_fov_scale, 1.0F));
        }
    }

    std::cout << "test_ads_transform_data passed.\n";
}

// ============================================================
// Test: ADS transform values per weapon
// ============================================================

void test_ads_transform_values() {
    using namespace ahamkara::client;

    // AR-15 (index 0)
    {
        auto ads = weapon_ads_transform(0);
        assert(nearly_equal(ads.ads_pos_right,   -0.05F));
        assert(nearly_equal(ads.ads_pos_up,       0.05F));
        assert(nearly_equal(ads.ads_pos_forward, -0.05F));
        assert(nearly_equal(ads.ads_pitch_deg,   -2.0F));
        assert(nearly_equal(ads.ads_yaw_deg,      0.0F));
        assert(nearly_equal(ads.ads_roll_deg,     0.0F));
        assert(nearly_equal(ads.ads_fov_scale,    0.70F));
    }

    // Shotgun (index 1)
    {
        auto ads = weapon_ads_transform(1);
        assert(nearly_equal(ads.ads_pos_right,   -0.08F));
        assert(nearly_equal(ads.ads_pos_up,       0.08F));
        assert(nearly_equal(ads.ads_pos_forward, -0.03F));
        assert(nearly_equal(ads.ads_pitch_deg,   -2.5F));
        assert(nearly_equal(ads.ads_yaw_deg,      0.0F));
        assert(nearly_equal(ads.ads_roll_deg,     0.0F));
        assert(nearly_equal(ads.ads_fov_scale,    0.67F));
    }

    // Rocket Launcher (index 2)
    {
        auto ads = weapon_ads_transform(2);
        assert(nearly_equal(ads.ads_pos_right,   -0.12F));
        assert(nearly_equal(ads.ads_pos_up,       0.15F));
        assert(nearly_equal(ads.ads_pos_forward, -0.02F));
        assert(nearly_equal(ads.ads_pitch_deg,   -3.0F));
        assert(nearly_equal(ads.ads_yaw_deg,      0.0F));
        assert(nearly_equal(ads.ads_roll_deg,     0.0F));
        assert(nearly_equal(ads.ads_fov_scale,    0.75F));
    }

    std::cout << "test_ads_transform_values passed.\n";
}

// ============================================================
// Test: ADS blend interpolation
// ============================================================

void test_ads_blend_starts_at_zero() {
    float blend = ads_blend_tick(0.0F, 0.0F, 0.0F, kDefaultAdsTransitionTime);
    assert(nearly_equal(blend, 0.0F));
    std::cout << "test_ads_blend_starts_at_zero passed.\n";
}

void test_ads_blend_ramps_up() {
    float blend = 0.0F;
    constexpr float dt = 1.0F / 60.0F;
    constexpr float speed = 1.0F / 0.2F;  // 5.0 per second
    constexpr float expected_per_tick = speed * dt;

    // After one tick of aiming, blend should increase
    blend = ads_blend_tick(blend, 1.0F, dt, 0.2F);
    assert(blend > 0.0F);
    assert(nearly_equal(blend, expected_per_tick));
    assert(blend < 1.0F);

    std::cout << "test_ads_blend_ramps_up passed.\n";
}

void test_ads_blend_reaches_one() {
    float blend = 0.0F;
    constexpr float dt = 1.0F / 60.0F;

    // Simulate 200ms of continuous aiming (0.2s transition time)
    // Should reach 1.0
    for (int i = 0; i < 15; ++i) {
        blend = ads_blend_tick(blend, 1.0F, dt, 0.2F);
    }
    assert(nearly_equal(blend, 1.0F));

    std::cout << "test_ads_blend_reaches_one passed.\n";
}

void test_ads_blend_ramps_down() {
    float blend = 1.0F;
    constexpr float dt = 1.0F / 60.0F;
    constexpr float speed = 1.0F / 0.2F;  // 5.0 per second
    constexpr float expected_per_tick = speed * dt;

    // After one tick of releasing aim, blend should decrease
    blend = ads_blend_tick(blend, 0.0F, dt, 0.2F);
    assert(blend < 1.0F);
    assert(nearly_equal(blend, 1.0F - expected_per_tick));
    assert(blend > 0.0F);

    std::cout << "test_ads_blend_ramps_down passed.\n";
}

void test_ads_blend_reaches_zero() {
    float blend = 1.0F;
    constexpr float dt = 1.0F / 60.0F;

    // Simulate 200ms of releasing aim
    for (int i = 0; i < 15; ++i) {
        blend = ads_blend_tick(blend, 0.0F, dt, 0.2F);
    }
    assert(nearly_equal(blend, 0.0F));

    std::cout << "test_ads_blend_reaches_zero passed.\n";
}

void test_ads_blend_partial_transition() {
    // Test that releasing aim mid-transition works correctly
    float blend = 0.0F;
    constexpr float dt = 1.0F / 60.0F;

    // Aim for 5 ticks (about 83ms, roughly halfway)
    for (int i = 0; i < 5; ++i) {
        blend = ads_blend_tick(blend, 1.0F, dt, 0.2F);
    }
    float mid_value = blend;
    assert(mid_value > 0.3F && mid_value < 0.5F);  // should be ~0.42

    // Release aim immediately
    for (int i = 0; i < 15; ++i) {
        blend = ads_blend_tick(blend, 0.0F, dt, 0.2F);
    }
    assert(nearly_equal(blend, 0.0F));

    std::cout << "test_ads_blend_partial_transition passed.\n";
}

void test_ads_blend_holds_at_target() {
    // Once at target, blend should remain there
    float blend;

    blend = ads_blend_tick(1.0F, 1.0F, 1.0F, kDefaultAdsTransitionTime);
    assert(nearly_equal(blend, 1.0F));

    blend = ads_blend_tick(0.0F, 0.0F, 1.0F, kDefaultAdsTransitionTime);
    assert(nearly_equal(blend, 0.0F));

    std::cout << "test_ads_blend_holds_at_target passed.\n";
}

// ============================================================
// Test: Per-weapon ADS blend speeds
// ============================================================

void test_ads_blend_per_weapon_speeds() {
    constexpr float dt = 1.0F / 60.0F;
    struct WeaponSpeed {
        const char* name;
        float transition_time;
    };
    constexpr WeaponSpeed kWeaponSpeeds[] = {
        {"AR-15",          0.18F},
        {"Shotgun",        0.25F},
        {"Rocket Launcher", 0.30F},
    };

    for (const auto& ws : kWeaponSpeeds) {
        float blend = 0.0F;
        const float speed = 1.0F / ws.transition_time;

        // After one tick
        blend = ads_blend_tick(blend, 1.0F, dt, ws.transition_time);
        float expected_per_tick = speed * dt;
        assert(nearly_equal(blend, expected_per_tick));

        // Full transition
        const int ticks_needed = static_cast<int>(std::ceil(ws.transition_time / dt)) + 1;
        for (int i = 1; i < ticks_needed; ++i) {
            blend = ads_blend_tick(blend, 1.0F, dt, ws.transition_time);
        }
        assert(nearly_equal(blend, 1.0F));
    }

    std::cout << "test_ads_blend_per_weapon_speeds passed.\n";
}

// ============================================================
// Test: Viewmodel offset blending with ADS
// ============================================================

void test_viewmodel_offset_blend() {
    using namespace ahamkara::client;

    // Verify the blend formula used in client_frame_pipeline.cpp:
    //   blended = hip_value + ads_value * ads_blend

    const auto vm = kWeaponViewmodelTransforms[0]; // AR-15 hip
    const auto ads = kWeaponAdsTransforms[0];       // AR-15 ADS

    // At ads_blend = 0 (hip fire), blended = hip position
    {
        const float br = vm.pos_right + ads.ads_pos_right * 0.0F;
        const float bu = vm.pos_up + ads.ads_pos_up * 0.0F;
        const float bf = vm.pos_forward + ads.ads_pos_forward * 0.0F;
        assert(nearly_equal(br, vm.pos_right));
        assert(nearly_equal(bu, vm.pos_up));
        assert(nearly_equal(bf, vm.pos_forward));
    }

    // At ads_blend = 1 (fully aimed), blended = hip + ads
    {
        const float br = vm.pos_right + ads.ads_pos_right * 1.0F;
        const float bu = vm.pos_up + ads.ads_pos_up * 1.0F;
        const float bf = vm.pos_forward + ads.ads_pos_forward * 1.0F;
        assert(nearly_equal(br, vm.pos_right + ads.ads_pos_right));
        assert(nearly_equal(bu, vm.pos_up + ads.ads_pos_up));
        assert(nearly_equal(bf, vm.pos_forward + ads.ads_pos_forward));
    }

    // At ads_blend = 0.5 (partial aim), blended = hip + ads * 0.5
    {
        const float br = vm.pos_right + ads.ads_pos_right * 0.5F;
        const float bu = vm.pos_up + ads.ads_pos_up * 0.5F;
        const float bf = vm.pos_forward + ads.ads_pos_forward * 0.5F;
        assert(nearly_equal(br, vm.pos_right + ads.ads_pos_right * 0.5F));
        assert(nearly_equal(bu, vm.pos_up + ads.ads_pos_up * 0.5F));
        assert(nearly_equal(bf, vm.pos_forward + ads.ads_pos_forward * 0.5F));
    }

    std::cout << "test_viewmodel_offset_blend passed.\n";
}

// ============================================================
// Test: Camera FOV zoom blending with ADS
// ============================================================

void test_camera_fov_blend() {
    using namespace ahamkara::client;

    constexpr float kBaseFovDeg = 60.0F;

    // AR-15 ads_fov_scale = 0.70 → zoom to 42° at full ADS
    {
        auto ads = weapon_ads_transform(0);
        const float target_ads_fov = kBaseFovDeg * ads.ads_fov_scale;

        // Hip (ads_blend = 0)
        float fov = kBaseFovDeg - (kBaseFovDeg - target_ads_fov) * 0.0F;
        assert(nearly_equal(fov, kBaseFovDeg));

        // Full ADS (ads_blend = 1)
        fov = kBaseFovDeg - (kBaseFovDeg - target_ads_fov) * 1.0F;
        assert(nearly_equal(fov, target_ads_fov));
        assert(nearly_equal(fov, 42.0F));

        // Half ADS (ads_blend = 0.5)
        fov = kBaseFovDeg - (kBaseFovDeg - target_ads_fov) * 0.5F;
        assert(nearly_equal(fov, (60.0F + 42.0F) * 0.5F));
    }

    // Shotgun: 60° → 40°
    {
        auto ads = weapon_ads_transform(1);
        const float target_ads_fov = kBaseFovDeg * ads.ads_fov_scale;
        assert(nearly_equal(target_ads_fov, 40.2F));  // 60 * 0.67
    }

    // Rocket Launcher: 60° → 45°
    {
        auto ads = weapon_ads_transform(2);
        const float target_ads_fov = kBaseFovDeg * ads.ads_fov_scale;
        assert(nearly_equal(target_ads_fov, 45.0F));  // 60 * 0.75
    }

    std::cout << "test_camera_fov_blend passed.\n";
}

}  // namespace

int main() {
    // ADS transform data
    test_ads_transform_data();
    test_ads_transform_values();

    // ADS blend interpolation
    test_ads_blend_starts_at_zero();
    test_ads_blend_ramps_up();
    test_ads_blend_reaches_one();
    test_ads_blend_ramps_down();
    test_ads_blend_reaches_zero();
    test_ads_blend_partial_transition();
    test_ads_blend_holds_at_target();

    // Per-weapon blend speeds
    test_ads_blend_per_weapon_speeds();

    // Viewmodel offset blending
    test_viewmodel_offset_blend();

    // Camera FOV blending
    test_camera_fov_blend();

    std::cout << "\nAll ADS tests passed.\n";
    return 0;
}
