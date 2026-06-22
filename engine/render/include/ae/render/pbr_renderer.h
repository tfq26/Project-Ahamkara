#pragma once

#include "ae/render/render_backend.h"
#include "ae/render/shadow_pass.h"
#include <memory>
#include <vector>

namespace ae::render {

struct PbrLight {
    float direction[3] = {0.5F, -1.0F, -0.3F};
    float color[3] = {1.0F, 0.95F, 0.85F};
    float ambient = 0.08F;
};

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

    void begin_frame(const float* view_matrix, const float* projection_matrix,
                     const float* camera_position, ShadowPass* shadow);
    void submit(const PbrDrawCall& draw_call);
    void end_frame();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ae::render
