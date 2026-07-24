#pragma once

#include "ae/render/atmosphere_pass.h"
#include "ae/render/color_grading.h"
#include "ae/render/render_backend.h"
#include "ae/render/shadow_pass.h"
#include <memory>
#include <vector>

namespace ae::render {

// ── Light types ─────────────────────────────────────────────────────────────

enum class LightType : int {
    Directional = 0,
    Point       = 1,
    Spot        = 2
};

struct PbrLight {
    LightType type = LightType::Directional;
    float direction[3] = {0.5F, -1.0F, -0.3F};
    float position[3] = {0.0F, 0.0F, 0.0F};
    float color[3] = {1.0F, 0.95F, 0.85F};
    float intensity = 1.0F;
    float ambient = 0.08F;
    float range = 50.0F;
    bool cast_shadows = true;
};

constexpr int kMaxDirectionalLights = 4;
constexpr int kMaxPointLights = 8;
constexpr int kMaxLights = kMaxDirectionalLights + kMaxPointLights;
constexpr int kCsmCascadeCount = 3;

constexpr int kShOrder = 3;                  // 3rd-order SH = 9 coefficients
constexpr int kShCoeffCount = kShOrder * kShOrder; // 9
constexpr int kMaxReflectionProbes = 4;

struct ReflectionProbe {
    TextureHandle cubemap;    // pre-filtered environment cubemap
    float position[3] = {0, 0, 0};
    float influence_radius = 50.0F;
    float intensity = 1.0F;
};

struct PbrDrawCall {
    GpuMesh* mesh = nullptr;
    const float* model_matrix = nullptr;  // 16 floats, column-major
    float albedo[3] = {1, 1, 1};
    float metallic = 0.0F;
    float roughness = 0.5F;
    float emissive_color[3] = {0, 0, 0};
    float emissive_intensity = 0.0F;
    TextureHandle albedo_map;
    TextureHandle normal_map;
    TextureHandle orm_map;
    TextureHandle emissive_map;
    const float* joint_matrices = nullptr;
    int joint_count = 0;
};

class PbrRenderer {
public:
    PbrRenderer();
    ~PbrRenderer();

    bool initialize(RenderBackend* backend);
    void shutdown();

    void set_lights(const PbrLight* lights, int count);
    void set_cascade_splits(const float* splits);

    // Ambient / IBL
    void set_ambient_sh(const float* sh_coefficients);  // 9 floats (3rd-order)
    void set_ambient_sky_ground(const float sky_color[3], const float ground_color[3]);
    void set_reflection_probes(const ReflectionProbe* probes, int count);
    void set_ao_texture(TextureHandle ao_tex);
    void set_color_grading(const ColorGradingParams& params);
    void set_fog(const FogParams& params);

    // SSAO
    void set_ssao_enabled(bool enabled);
    // set_ao_texture shared with Ambient/IBL (above)

    // TAA
    void set_frame_index(int index);

    void begin_frame(const float* view_matrix, const float* projection_matrix,
                     const float* camera_position, ShadowPass* shadow);
    void submit(const PbrDrawCall& draw_call);
    /// Returns the (possibly jittered) projection matrix used this frame.
    const float* jittered_projection() const;

    void end_frame();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ae::render
