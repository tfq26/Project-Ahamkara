#pragma once

#include "ae/render/render_backend.h"
#include <memory>

namespace ae::render {

/// Temporal anti-aliasing state.
/// Holds jitter sequence, previous frame data, and motion vector uniforms.
class TemporalAA {
public:
    TemporalAA();
    ~TemporalAA();

    bool initialize(RenderBackend* backend, int width, int height);
    void shutdown();

    /// Compute jittered projection matrix. Call before rendering.
    /// Returns the jitter offsets in normalized pixel coords via jitter_xy.
    void jitter_projection(const float* base_proj, int frame_index,
                           int width, int height,
                           float out_jittered[16], float jitter_xy[2]);

    /// Store the previous frame's view/projection for motion vector computation.
    void store_prev_matrices(const float* view, const float* proj);

    const float* prev_view() const { return prev_view_; }
    const float* prev_proj() const { return prev_proj_; }

private:
    float prev_view_[16] = {};
    float prev_proj_[16] = {};
    bool has_history_ = false;
};

} // namespace ae::render
