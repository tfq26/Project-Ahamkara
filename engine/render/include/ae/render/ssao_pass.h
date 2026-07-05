#pragma once

#include "ae/render/render_backend.h"
#include <cstdint>
#include <memory>

namespace ae::render {

/// Screen-space ambient occlusion pass.
/// Samples depth around each pixel and computes an occlusion factor.
class SsaoPass {
public:
    SsaoPass();
    ~SsaoPass();

    SsaoPass(const SsaoPass&) = delete;
    SsaoPass& operator=(const SsaoPass&) = delete;

    bool initialize(RenderBackend* backend, int width, int height);
    void shutdown();

    /// Compute SSAO from a depth texture. Result is written to the internal
    /// AO texture. The depth texture must be bound at the given slot.
    void render(std::uint32_t depth_texture, int depth_slot,
                const float* projection_matrix, int width, int height);

    /// Bind the computed AO result to a texture slot.
    void bind_ao_result(int slot);

    TextureHandle ao_texture() const { return ao_tex_; }

    float radius = 0.5F;
    float power = 2.0F;
    float bias = 0.025F;

private:
    RenderBackend* backend_ = nullptr;
    TextureHandle ao_tex_;
    std::uint32_t fbo_ = 0;
    std::uint32_t shader_ = 0;
    std::uint32_t quad_vao_ = 0;
    std::uint32_t quad_vbo_ = 0;
    int u_proj_ = -1, u_depth_tex_ = -1;
    int u_radius_ = -1, u_power_ = -1, u_bias_ = -1;
};

} // namespace ae::render
