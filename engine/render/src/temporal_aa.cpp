#include "ae/render/temporal_aa.h"
#include "ae/core/log.h"
#include <cmath>
#include <cstring>

#define AE_LOG_CATEGORY "Render"

namespace ae::render {

TemporalAA::TemporalAA() = default;
TemporalAA::~TemporalAA() { shutdown(); }

bool TemporalAA::initialize(RenderBackend* backend, int width, int height) {
    (void)backend;
    (void)width;
    (void)height;
    has_history_ = false;
    return true;
}

void TemporalAA::shutdown() {
    has_history_ = false;
}

void TemporalAA::jitter_projection(const float* base_proj, int frame_index,
                                    int width, int height,
                                    float out_jittered[16], float jitter_xy[2]) {
    // Halton(2,3) jitter sequence
    static const float halton2[] = {
        0.5f, 0.25f, 0.75f, 0.125f, 0.625f,
        0.375f, 0.875f, 0.0625f, 0.5625f, 0.3125f,
        0.8125f, 0.1875f, 0.6875f, 0.4375f, 0.9375f,
        0.03125f, 0.53125f, 0.28125f, 0.78125f, 0.15625f
    };
    static const float halton3[] = {
        0.33333f, 0.66667f, 0.11111f, 0.44444f, 0.77778f,
        0.22222f, 0.55556f, 0.88889f, 0.037037f, 0.37037f,
        0.70370f, 0.14815f, 0.48148f, 0.81481f, 0.25926f,
        0.59259f, 0.92593f, 0.074074f, 0.40741f, 0.74074f
    };
    int idx = frame_index % 16;
    float jx = (halton2[idx] - 0.5f) * 2.0f / static_cast<float>(width);
    float jy = (halton3[idx] - 0.5f) * 2.0f / static_cast<float>(height);

    std::memcpy(out_jittered, base_proj, 16 * sizeof(float));
    out_jittered[8] += jx;
    out_jittered[9] += jy;

    jitter_xy[0] = jx;
    jitter_xy[1] = jy;
}

void TemporalAA::store_prev_matrices(const float* view, const float* proj) {
    std::memcpy(prev_view_, view, 16 * sizeof(float));
    std::memcpy(prev_proj_, proj, 16 * sizeof(float));
    has_history_ = true;
}

} // namespace ae::render
