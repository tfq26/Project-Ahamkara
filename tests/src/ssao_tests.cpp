#include "ae/render/ssao_pass.h"
#include "ae/render/temporal_aa.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <string>

namespace {

bool near_eq(float lhs, float rhs, float eps = 0.0001F) {
    return std::fabs(lhs - rhs) < eps;
}

int fail(const std::string& message) {
    std::cerr << "ssao_tests failed: " << message << '\n';
    return 1;
}

// ── SsaoPass lifecycle ─────────────────────────────────────────────

int test_ssaopass_default_construction() {
    ae::render::SsaoPass pass;
    // Default parameters
    if (!near_eq(pass.radius, 0.5F)) return fail("default radius should be 0.5");
    if (!near_eq(pass.power, 2.0F))  return fail("default power should be 2.0");
    if (!near_eq(pass.bias, 0.025F)) return fail("default bias should be 0.025");
    return 0;
}

int test_ssaopass_parameter_setters() {
    ae::render::SsaoPass pass;
    pass.radius = 1.0F;
    pass.power  = 1.5F;
    pass.bias   = 0.01F;

    if (!near_eq(pass.radius, 1.0F))  return fail("radius setter failed");
    if (!near_eq(pass.power, 1.5F))   return fail("power setter failed");
    if (!near_eq(pass.bias, 0.01F))   return fail("bias setter failed");
    return 0;
}

int test_ssaopass_double_shutdown() {
    ae::render::SsaoPass pass;
    // Calling shutdown on an uninitialized pass should be safe
    pass.shutdown();
    pass.shutdown();
    return 0;
}

int test_ssaopass_ao_texture_default() {
    ae::render::SsaoPass pass;
    // Before initialize, ao_texture() should return a zero handle
    auto tex = pass.ao_texture();
    if (tex.id != 0) return fail("ao_texture should be 0 before init");
    return 0;
}

// ── TemporalAA jitter ──────────────────────────────────────────────

int test_taa_default_construction() {
    ae::render::TemporalAA taa;
    if (taa.prev_view() != nullptr) return fail("prev_view should be null before init");
    if (taa.prev_proj() != nullptr) return fail("prev_proj should be null before init");
    return 0;
}

int test_taa_initialize_shutdown() {
    ae::render::TemporalAA taa;
    // initialize with null backend (it doesn't use it)
    bool ok = taa.initialize(nullptr, 1920, 1080);
    if (!ok) return fail("initialize should succeed");
    taa.shutdown();
    // double shutdown should be safe
    taa.shutdown();
    return 0;
}

int test_taa_jitter_repeatable() {
    ae::render::TemporalAA taa;

    float base_proj[16] = {};
    base_proj[0] = 1.0F; base_proj[5] = 1.0F; base_proj[10] = -1.0F; base_proj[15] = 1.0F;

    float out1[16], out2[16];
    float jitter1[2], jitter2[2];

    // Same frame index should produce identical jitter
    taa.jitter_projection(base_proj, 0, 1920, 1080, out1, jitter1);
    taa.jitter_projection(base_proj, 0, 1920, 1080, out2, jitter2);

    if (jitter1[0] != jitter2[0]) return fail("jitter x should be repeatable for same frame");
    if (jitter1[1] != jitter2[1]) return fail("jitter y should be repeatable for same frame");

    for (int i = 0; i < 16; ++i) {
        if (!near_eq(out1[i], out2[i])) return fail("jittered projection should be repeatable");
    }

    return 0;
}

int test_taa_jitter_changes_per_frame() {
    ae::render::TemporalAA taa;

    float base_proj[16] = {};
    base_proj[0] = 1.0F; base_proj[5] = 1.0F; base_proj[10] = -1.0F; base_proj[15] = 1.0F;

    float out_prev[16], out_cur[16];
    float jitter_prev[2], jitter_cur[2];

    taa.jitter_projection(base_proj, 0, 1920, 1080, out_prev, jitter_prev);
    taa.jitter_projection(base_proj, 1, 1920, 1080, out_cur, jitter_cur);

    // Consecutive frames should differ (Halton(2,3) changes each step)
    if (jitter_prev[0] == jitter_cur[0] && jitter_prev[1] == jitter_cur[1]) {
        return fail("jitter should differ between frames 0 and 1");
    }

    return 0;
}

int test_taa_jitter_only_affects_projection_xy() {
    ae::render::TemporalAA taa;

    float base_proj[16] = {};
    base_proj[0] = 1.0F; base_proj[5] = 1.0F; base_proj[10] = -1.0F; base_proj[15] = 1.0F;

    float out[16];
    float jitter[2];

    taa.jitter_projection(base_proj, 5, 1920, 1080, out, jitter);

    // Only elements [8] and [9] (the projection offset) should differ from identity
    // out[8] and out[9] are the jittered offsets
    for (int i = 0; i < 16; ++i) {
        if (i == 8 || i == 9) continue;
        if (!near_eq(out[i], base_proj[i])) return fail("jitter should only affect elements 8 and 9");
    }

    return 0;
}

int test_taa_store_prev_matrices() {
    ae::render::TemporalAA taa;

    float view[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 10,20,30,1};
    float proj[16] = {2,0,0,0, 0,2,0,0, 0,0,-1,0, 0,0,0,1};

    taa.store_prev_matrices(view, proj);

    if (taa.prev_view() == nullptr) return fail("prev_view should not be null after store");
    if (taa.prev_proj() == nullptr) return fail("prev_proj should not be null after store");

    for (int i = 0; i < 16; ++i) {
        if (!near_eq(taa.prev_view()[i], view[i])) return fail("prev_view mismatch at index " + std::to_string(i));
        if (!near_eq(taa.prev_proj()[i], proj[i])) return fail("prev_proj mismatch at index " + std::to_string(i));
    }

    return 0;
}



}  // namespace

int main() {
    int rc;

    rc = test_ssaopass_default_construction();
    if (rc != 0) return rc;

    rc = test_ssaopass_parameter_setters();
    if (rc != 0) return rc;

    rc = test_ssaopass_double_shutdown();
    if (rc != 0) return rc;

    rc = test_ssaopass_ao_texture_default();
    if (rc != 0) return rc;

    rc = test_taa_default_construction();
    if (rc != 0) return rc;

    rc = test_taa_initialize_shutdown();
    if (rc != 0) return rc;

    rc = test_taa_jitter_repeatable();
    if (rc != 0) return rc;

    rc = test_taa_jitter_changes_per_frame();
    if (rc != 0) return rc;

    rc = test_taa_jitter_only_affects_projection_xy();
    if (rc != 0) return rc;

    rc = test_taa_store_prev_matrices();
    if (rc != 0) return rc;

    std::cout << "ssao_tests passed\n";
    return 0;
}
