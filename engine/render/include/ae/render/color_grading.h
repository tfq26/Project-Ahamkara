#pragma once

namespace ae::render {

/// Color grading parameters for post-processing.
struct ColorGradingParams {
    float exposure = 1.0F;
    float contrast = 1.0F;
    float saturation = 1.0F;
    float brightness = 0.0F;

    // Vignette
    float vignette_strength = 0.0F;
    float vignette_radius = 0.75F;

    // Tonemapping selection: 0 = none, 1 = Reinhard, 2 = ACES (approx)
    int tonemap_mode = 1;
};

} // namespace ae::render
