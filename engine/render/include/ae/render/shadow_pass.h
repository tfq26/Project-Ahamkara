#pragma once

#include "ae/render/render_backend.h"
#include <memory>

namespace ae::render {

constexpr int kMaxCsmCascades = 4;

struct ShadowBoxCaster {
    float min[3] {0.0F, 0.0F, 0.0F};
    float max[3] {0.0F, 0.0F, 0.0F};
};

// Stores cascade frustum splits and per-cascade light-space matrices.
struct CsmData {
    float splits[kMaxCsmCascades] = {10.0F, 30.0F, 60.0F, 100.0F};
    float light_space_matrices[kMaxCsmCascades][16] = {};
    int cascade_count = 3;
};

class ShadowPass {
public:
    ShadowPass();
    ~ShadowPass();

    bool initialize(RenderBackend* backend, int resolution = 2048);
    void shutdown();

    // Single-cascade pass (backward compat)
    void begin_pass(const float* light_view, const float* light_projection);
    // CSM pass — renders all cascades
    void begin_csm_pass(const CsmData& csm, const float* cam_view,
                        const float* cam_proj, const float* cam_pos);
    void end_pass();

    void submit_box_caster(const ShadowBoxCaster& caster);
    // Bind cascades as a 2D array texture; cascade_index selects the active
    // cascade for rendering (during CSM pass) or all cascades (during sampling).
    void bind_shadow_map(int slot, int cascade_index = -1);

    [[nodiscard]] int resolution() const { return 2048; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ae::render
