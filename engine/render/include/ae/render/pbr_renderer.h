#pragma once

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

struct PbrDrawCall {
    GpuMesh* mesh = nullptr;
    const float* model_matrix = nullptr;  // 16 floats, column-major
    float albedo[3] = {1, 1, 1};
    float metallic = 0.0F;
    float roughness = 0.5F;
    TextureHandle albedo_map;
    TextureHandle normal_map;
    TextureHandle orm_map;
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

    void begin_frame(const float* view_matrix, const float* projection_matrix,
                     const float* camera_position, ShadowPass* shadow);
    void submit(const PbrDrawCall& draw_call);
    void end_frame();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ae::render
