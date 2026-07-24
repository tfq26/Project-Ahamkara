/// Unit tests for ae::audio::AudioEngine — occlusion, bus volume, and category
/// mapping.  Does NOT require a GPU or real audio playback.
#include "ae/audio/audio_engine.h"

#include <cmath>
#include <iostream>
#include <cstdlib>

namespace {

// ── Test helpers ─────────────────────────────────────────────────────────────

int failures = 0;

void check(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << "FAIL: " << msg << "\n";
        ++failures;
    } else {
        std::cout << "  ok: " << msg << "\n";
    }
}

// ── Mock raycast callbacks ───────────────────────────────────────────────────
//
// Simulate various occlusion scenarios using simple static configuration.

/// Reports a hit at 5 meters from source if within range.
bool mock_hit_at_5m(float ox, float oy, float oz,
                    float dx, float dy, float dz,
                    float max_dist, float& out_hit_dist) {
    out_hit_dist = 5.0F;
    return 5.0F < max_dist;
}

/// Always reports a hit at 1 meter (close to source → high occlusion).
bool mock_hit_at_1m(float, float, float,
                    float, float, float,
                    float, float& out_hit_dist) {
    out_hit_dist = 1.0F;
    return true;
}

/// Never reports a hit.
bool mock_no_hit(float, float, float,
                 float, float, float,
                 float, float&) {
    return false;
}

// ── Tests ────────────────────────────────────────────────────────────────────

void test_no_callback_returns_zero() {
    ae::audio::AudioEngine engine;
    // Without a callback, occlusion should always be 0.0
    float occ = engine.check_occlusion(0, 0, 0, 10, 0, 0);
    check(std::fabs(occ) < 0.001F, "no callback → occlusion = 0");
}

void test_no_hit_returns_zero() {
    ae::audio::AudioEngine engine;
    engine.set_occlusion_raycast(mock_no_hit);
    float occ = engine.check_occlusion(0, 0, 0, 10, 0, 0);
    check(std::fabs(occ) < 0.001F, "no raycast hit → occlusion = 0");
}

void test_hit_returns_positive_occlusion() {
    ae::audio::AudioEngine engine;
    engine.set_occlusion_raycast(mock_hit_at_5m);
    // Wall at 5m, listener at 10m → 1.0 - (5/10) = 0.5
    float occ = engine.check_occlusion(0, 0, 0, 10, 0, 0);
    check(std::fabs(occ - 0.5F) < 0.01F, "hit halfway → occlusion ≈ 0.5");
}

void test_hit_close_to_source_returns_high_occlusion() {
    ae::audio::AudioEngine engine;
    engine.set_occlusion_raycast(mock_hit_at_1m);
    // Wall at 1m, listener at 10m → 1.0 - (1/10) = 0.9
    float occ = engine.check_occlusion(0, 0, 0, 10, 0, 0);
    check(std::fabs(occ - 0.9F) < 0.01F, "hit close to source → occlusion ≈ 0.9");
}

void test_hit_at_listener_returns_minimal_occlusion() {
    ae::audio::AudioEngine engine;
    engine.set_occlusion_raycast(mock_hit_at_5m);
    // Wall at 5m, listener at 5m → 1.0 - (5/5) = 0.0
    float occ = engine.check_occlusion(0, 0, 0, 5.0F, 0, 0);
    check(std::fabs(occ) < 0.01F, "wall at listener distance → occlusion ≈ 0");
}

void test_zero_distance_returns_zero() {
    ae::audio::AudioEngine engine;
    engine.set_occlusion_raycast(mock_hit_at_5m);
    float occ = engine.check_occlusion(0, 0, 0, 0, 0, 0);
    check(std::fabs(occ) < 0.001F, "zero source-listener distance → occlusion = 0");
}

void test_set_category_volume_maps_names() {
    ae::audio::AudioEngine engine;

    // set_category_volume("master", 0.5F) should set master bus volume
    // Since we can't easily get bus volume without initialize(), we just
    // verify the method doesn't crash and maps known categories.
    engine.set_category_volume("master", 0.5F);
    engine.set_category_volume("sfx", 0.3F);
    engine.set_category_volume("music", 0.8F);
    engine.set_category_volume("unknown_category", 0.5F); // should log warning, not crash

    // Bus volume getters should reflect the set values
    float mv = engine.get_bus_volume(ae::audio::AudioBus::Master);
    float sv = engine.get_bus_volume(ae::audio::AudioBus::SFX);
    float muv = engine.get_bus_volume(ae::audio::AudioBus::Music);

    check(std::fabs(mv - 0.5F) < 0.01F, "master category → master bus volume = 0.5");
    check(std::fabs(sv - 0.3F) < 0.01F, "sfx category → sfx bus volume = 0.3");
    check(std::fabs(muv - 0.8F) < 0.01F, "music category → music bus volume = 0.8");
}

void test_set_bus_volume_independent() {
    ae::audio::AudioEngine engine;
    engine.set_bus_volume(ae::audio::AudioBus::SFX, 0.25F);
    engine.set_bus_volume(ae::audio::AudioBus::Weapon, 0.75F);

    float sfx = engine.get_bus_volume(ae::audio::AudioBus::SFX);
    float wpn = engine.get_bus_volume(ae::audio::AudioBus::Weapon);
    float mst = engine.get_bus_volume(ae::audio::AudioBus::Master);

    check(std::fabs(sfx - 0.25F) < 0.01F, "SFX bus volume = 0.25");
    check(std::fabs(wpn - 0.75F) < 0.01F, "Weapon bus volume = 0.75");
    check(std::fabs(mst - 1.0F) < 0.01F, "Master bus remains 1.0 (independent)");
}

} // anonymous namespace

int main() {
    std::cout << "audio_engine_tests:\n";

    test_no_callback_returns_zero();
    test_no_hit_returns_zero();
    test_hit_returns_positive_occlusion();
    test_hit_close_to_source_returns_high_occlusion();
    test_hit_at_listener_returns_minimal_occlusion();
    test_zero_distance_returns_zero();
    test_set_category_volume_maps_names();
    test_set_bus_volume_independent();

    if (failures == 0) {
        std::cout << "audio_engine_tests: all ok\n";
        return 0;
    }
    std::cerr << "audio_engine_tests: " << failures << " test(s) failed\n";
    return 1;
}
