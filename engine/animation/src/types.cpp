#include "ae/animation/types.h"

#include <algorithm>
#include <cmath>

namespace ae::animation {

// ============================================================
// JointTransform
// ============================================================

skeleton::Mat4 JointTransform::to_mat4() const {
    // T * R * S
    skeleton::Mat4 t = skeleton::Mat4::translation(tx, ty, tz);
    skeleton::Mat4 r = skeleton::Mat4::rotation_quat(qx, qy, qz, qw);
    skeleton::Mat4 s = skeleton::Mat4::scale(sx, sy, sz);
    return t * r * s;
}

JointTransform JointTransform::blend(const JointTransform& a, const JointTransform& b, float t) {
    JointTransform result;

    // Linear interpolation for translation
    result.tx = a.tx + (b.tx - a.tx) * t;
    result.ty = a.ty + (b.ty - a.ty) * t;
    result.tz = a.tz + (b.tz - a.tz) * t;

    // Slerp for rotation
    float aq[4] = {a.qx, a.qy, a.qz, a.qw};
    float bq[4] = {b.qx, b.qy, b.qz, b.qw};
    float out_q[4];
    skeleton::quat_slerp(aq, bq, t, out_q);
    result.qx = out_q[0];
    result.qy = out_q[1];
    result.qz = out_q[2];
    result.qw = out_q[3];

    // Linear interpolation for scale
    result.sx = a.sx + (b.sx - a.sx) * t;
    result.sy = a.sy + (b.sy - a.sy) * t;
    result.sz = a.sz + (b.sz - a.sz) * t;

    return result;
}

JointTransform JointTransform::identity() {
    JointTransform result;
    result.tx = 0.0F; result.ty = 0.0F; result.tz = 0.0F;
    result.qx = 0.0F; result.qy = 0.0F; result.qz = 0.0F; result.qw = 1.0F;
    result.sx = 1.0F; result.sy = 1.0F; result.sz = 1.0F;
    return result;
}

// ============================================================
// AnimationPose
// ============================================================

void AnimationPose::compute_globals(const std::vector<int>& parent_indices) {
    const std::size_t count = local_transforms.size();
    global_matrices.resize(count);

    for (std::size_t j = 0; j < count; ++j) {
        skeleton::Mat4 local = local_transforms[j].to_mat4();

        int parent = (j < parent_indices.size()) ? parent_indices[j] : -1;
        if (parent >= 0 && static_cast<std::size_t>(parent) < count) {
            global_matrices[j] = global_matrices[static_cast<std::size_t>(parent)] * local;
        } else {
            global_matrices[j] = local;
        }
    }
}

// ============================================================
// ClipInstance
// ============================================================

void ClipInstance::advance(float dt) {
    if (!active || !clip || clip->duration_seconds <= 0.0F) {
        return;
    }

    time += dt * playback_speed;

    if (clip->looping) {
        // Wrap around
        while (time >= clip->duration_seconds) {
            time -= clip->duration_seconds;
        }
        while (time < 0.0F) {
            time += clip->duration_seconds;
        }
        finished = false;
    } else {
        if (time >= clip->duration_seconds) {
            time = clip->duration_seconds;
            finished = true;
        }
    }
}

void ClipInstance::reset() {
    time = 0.0F;
    finished = false;
}

float ClipInstance::normalized_time() const {
    if (!clip || clip->duration_seconds <= 0.0F) {
        return 0.0F;
    }
    return time / clip->duration_seconds;
}

// ============================================================
// BlendSpace1D
// ============================================================

bool BlendSpace1D::get_blend_pair(std::size_t& idx_a, std::size_t& idx_b, float& t) const {
    if (samples.size() < 2) return false;

    // Find the two samples bracketing current_parameter
    if (current_parameter <= samples[0].threshold) {
        idx_a = 0;
        idx_b = 0;
        t = 0.0F;
        return true;
    }

    if (current_parameter >= samples.back().threshold) {
        idx_a = samples.size() - 1;
        idx_b = samples.size() - 1;
        t = 0.0F;
        return true;
    }

    // Find first sample with threshold > current_parameter
    std::size_t i = 1;
    while (i < samples.size() && samples[i].threshold <= current_parameter) {
        ++i;
    }

    idx_a = i - 1;
    idx_b = i;
    float range = samples[idx_b].threshold - samples[idx_a].threshold;
    t = (range > 0.0F) ? (current_parameter - samples[idx_a].threshold) / range : 0.0F;
    return true;
}

// ============================================================
// BlendSpace2D
// ============================================================

bool BlendSpace2D::get_barycentric_blend(std::size_t& idx_a, std::size_t& idx_b,
                                          std::size_t& idx_c, float& wa, float& wb,
                                          float& wc) const {
    if (samples.size() < 3) return false;

    // Find the three closest samples by Euclidean distance in parameter space
    // Simple approach: find 3 nearest neighbors and compute barycentric weights
    float px = current_parameter_x;
    float py = current_parameter_y;

    // Find indices of 3 closest samples
    std::size_t best[3] = {0, 1, 2};
    float best_dist[3] = {
        std::sqrt((samples[0].threshold_x - px) * (samples[0].threshold_x - px) +
                  (samples[0].threshold_y - py) * (samples[0].threshold_y - py)),
        std::sqrt((samples[1].threshold_x - px) * (samples[1].threshold_x - px) +
                  (samples[1].threshold_y - py) * (samples[1].threshold_y - py)),
        std::sqrt((samples[2].threshold_x - px) * (samples[2].threshold_x - px) +
                  (samples[2].threshold_y - py) * (samples[2].threshold_y - py)),
    };

    for (std::size_t i = 3; i < samples.size(); ++i) {
        float dist = std::sqrt(
            (samples[i].threshold_x - px) * (samples[i].threshold_x - px) +
            (samples[i].threshold_y - py) * (samples[i].threshold_y - py));

        if (dist < best_dist[0]) {
            best_dist[2] = best_dist[1]; best[2] = best[1];
            best_dist[1] = best_dist[0]; best[1] = best[0];
            best_dist[0] = dist; best[0] = i;
        } else if (dist < best_dist[1]) {
            best_dist[2] = best_dist[1]; best[2] = best[1];
            best_dist[1] = dist; best[1] = i;
        } else if (dist < best_dist[2]) {
            best_dist[2] = dist; best[2] = i;
        }
    }

    idx_a = best[0];
    idx_b = best[1];
    idx_c = best[2];

    // Inverse distance weighting
    float inv_a = (best_dist[0] > 0.0001F) ? 1.0F / best_dist[0] : 1000.0F;
    float inv_b = (best_dist[1] > 0.0001F) ? 1.0F / best_dist[1] : 1000.0F;
    float inv_c = (best_dist[2] > 0.0001F) ? 1.0F / best_dist[2] : 1000.0F;
    float total = inv_a + inv_b + inv_c;

    wa = inv_a / total;
    wb = inv_b / total;
    wc = inv_c / total;

    return true;
}

}  // namespace ae::animation
