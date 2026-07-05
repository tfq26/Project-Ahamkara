#pragma once

#include "ae/render/render_backend.h"
#include <cstdint>
#include <memory>

namespace ae::render {

/// Time-of-day parameters for atmosphere/lighting.
struct TimeOfDay {
    float hour = 12.0F;           // 0-24
    float sun_angle = 45.0F;      // degrees above horizon
    float sun_color[3] = {1.0F, 0.95F, 0.85F};
    float sky_color[3] = {0.5F, 0.7F, 1.0F};
    float horizon_color[3] = {0.9F, 0.85F, 0.8F};
    float sun_intensity = 1.0F;
};

/// Fog parameters for height and aerial fog.
struct FogParams {
    float height_fog_density = 0.01F;
    float height_fog_height = 0.0F;   // Y-level where fog is thickest
    float height_fog_falloff = 10.0F; // how fast fog thins above height
    float aerial_fog_density = 0.001F;
    float fog_color[3] = {0.7F, 0.75F, 0.8F};
};

/// Atmosphere rendering pass (skybox).
class AtmospherePass {
public:
    AtmospherePass();
    ~AtmospherePass();

    AtmospherePass(const AtmospherePass&) = delete;
    AtmospherePass& operator=(const AtmospherePass&) = delete;

    bool initialize(RenderBackend* backend);
    void shutdown();

    /// Render the skybox/atmosphere.
    void render(const float* view_matrix, const float* projection_matrix,
                const TimeOfDay& tod);

    TimeOfDay time_of_day;
    FogParams fog;

private:
    RenderBackend* backend_ = nullptr;
    std::uint32_t shader_ = 0;
    std::uint32_t vao_ = 0;
    std::uint32_t vbo_ = 0;
    int u_view_proj_ = -1, u_sun_dir_ = -1;
    int u_sun_color_ = -1, u_sun_intensity_ = -1;
    int u_sky_color_ = -1, u_horizon_color_ = -1;
};

} // namespace ae::render
