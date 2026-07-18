#include "ae/render/color_grading.h"

#include <cmath>
#include <iostream>

namespace {

bool near_eq(float lhs, float rhs, float eps = 0.0001F) {
    return std::fabs(lhs - rhs) < eps;
}

int fail(const std::string& message) {
    std::cerr << "color_grading_tests failed: " << message << '\n';
    return 1;
}

int test_defaults() {
    ae::render::ColorGradingParams params;
    if (!near_eq(params.exposure, 1.0F))         return fail("default exposure should be 1.0");
    if (!near_eq(params.contrast, 1.0F))          return fail("default contrast should be 1.0");
    if (!near_eq(params.saturation, 1.0F))        return fail("default saturation should be 1.0");
    if (!near_eq(params.brightness, 0.0F))        return fail("default brightness should be 0.0");
    if (!near_eq(params.vignette_strength, 0.0F)) return fail("default vignette_strength should be 0.0");
    if (!near_eq(params.vignette_radius, 0.75F))  return fail("default vignette_radius should be 0.75");
    if (params.tonemap_mode != 1)                 return fail("default tonemap_mode should be 1 (Reinhard)");
    return 0;
}

int test_setters() {
    ae::render::ColorGradingParams params;
    params.exposure = 2.0F;
    params.contrast = 0.5F;
    params.saturation = 0.0F;
    params.brightness = 0.1F;
    params.vignette_strength = 0.5F;
    params.vignette_radius = 0.5F;
    params.tonemap_mode = 2;

    if (!near_eq(params.exposure, 2.0F))         return fail("exposure setter failed");
    if (!near_eq(params.contrast, 0.5F))          return fail("contrast setter failed");
    if (!near_eq(params.saturation, 0.0F))        return fail("saturation setter failed");
    if (!near_eq(params.brightness, 0.1F))        return fail("brightness setter failed");
    if (!near_eq(params.vignette_strength, 0.5F)) return fail("vignette_strength setter failed");
    if (!near_eq(params.vignette_radius, 0.5F))   return fail("vignette_radius setter failed");
    if (params.tonemap_mode != 2)                 return fail("tonemap_mode setter failed");
    return 0;
}

int test_tonemap_mode_values() {
    ae::render::ColorGradingParams params;

    params.tonemap_mode = 0;
    if (params.tonemap_mode != 0) return fail("tonemap_mode 0 failed");

    params.tonemap_mode = 1;
    if (params.tonemap_mode != 1) return fail("tonemap_mode 1 failed");

    params.tonemap_mode = 2;
    if (params.tonemap_mode != 2) return fail("tonemap_mode 2 failed");

    return 0;
}

int test_zero_exposure() {
    ae::render::ColorGradingParams params;
    params.exposure = 0.0F;
    if (!near_eq(params.exposure, 0.0F)) return fail("exposure set to 0 failed");
    return 0;
}

int test_negative_vignette() {
    ae::render::ColorGradingParams params;
    // Vignette strength can be negative (inverted effect)
    params.vignette_strength = -0.5F;
    if (!near_eq(params.vignette_strength, -0.5F)) return fail("negative vignette_strength failed");
    return 0;
}

}  // namespace

int main() {
    if (int rc = test_defaults(); rc != 0) return rc;
    if (int rc = test_setters(); rc != 0) return rc;
    if (int rc = test_tonemap_mode_values(); rc != 0) return rc;
    if (int rc = test_zero_exposure(); rc != 0) return rc;
    if (int rc = test_negative_vignette(); rc != 0) return rc;

    std::cout << "color_grading_tests passed\n";
    return 0;
}
