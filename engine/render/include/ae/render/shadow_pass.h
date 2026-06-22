#pragma once

#include "ae/render/render_backend.h"
#include <memory>

namespace ae::render {

struct ShadowBoxCaster {
    float min[3] {0.0F, 0.0F, 0.0F};
    float max[3] {0.0F, 0.0F, 0.0F};
};

class ShadowPass {
public:
    ShadowPass();
    ~ShadowPass();

    bool initialize(RenderBackend* backend, int resolution = 2048);
    void shutdown();

    void begin_pass(const float* light_view, const float* light_projection);
    void end_pass();
    void submit_box_caster(const ShadowBoxCaster& caster);
    void bind_shadow_map(int slot);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ae::render
