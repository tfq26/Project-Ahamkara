#include "ae/skeleton/types.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace ae::skeleton {

// ============================================================
// Mat4
// ============================================================

Mat4::Mat4() {
    m.fill(0.0F);
}

Mat4 Mat4::identity() {
    Mat4 result;
    result.m[0]  = 1.0F;  // col 0, row 0
    result.m[5]  = 1.0F;  // col 1, row 1
    result.m[10] = 1.0F;  // col 2, row 2
    result.m[15] = 1.0F;  // col 3, row 3
    return result;
}

Mat4 Mat4::translation(float x, float y, float z) {
    Mat4 result = identity();
    result.m[12] = x;  // col 3, row 0
    result.m[13] = y;  // col 3, row 1
    result.m[14] = z;  // col 3, row 2
    return result;
}

Mat4 Mat4::rotation_quat(float x, float y, float z, float w) {
    // Normalize quaternion
    float len = std::sqrt(x * x + y * y + z * z + w * w);
    if (len > 0.0F) {
        x /= len;
        y /= len;
        z /= len;
        w /= len;
    }

    float xx = x * x;
    float yy = y * y;
    float zz = z * z;
    float xy = x * y;
    float xz = x * z;
    float yz = y * z;
    float wx = w * x;
    float wy = w * y;
    float wz = w * z;

    Mat4 result = identity();
    // col 0
    result.m[0] = 1.0F - 2.0F * (yy + zz);
    result.m[1] = 2.0F * (xy + wz);
    result.m[2] = 2.0F * (xz - wy);
    // col 1
    result.m[4] = 2.0F * (xy - wz);
    result.m[5] = 1.0F - 2.0F * (xx + zz);
    result.m[6] = 2.0F * (yz + wx);
    // col 2
    result.m[8]  = 2.0F * (xz + wy);
    result.m[9]  = 2.0F * (yz - wx);
    result.m[10] = 1.0F - 2.0F * (xx + yy);

    return result;
}

Mat4 Mat4::scale(float sx, float sy, float sz) {
    Mat4 result = identity();
    result.m[0]  = sx;
    result.m[5]  = sy;
    result.m[10] = sz;
    return result;
}

Mat4 Mat4::operator*(const Mat4& other) const {
    Mat4 result;
    // result[col*4+row] = sum over k of this[k*4+row] * other[col*4+k]
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0F;
            for (int k = 0; k < 4; ++k) {
                sum += m[static_cast<std::size_t>(k * 4 + row)] *
                       other.m[static_cast<std::size_t>(col * 4 + k)];
            }
            result.m[static_cast<std::size_t>(col * 4 + row)] = sum;
        }
    }
    return result;
}

// ============================================================
// Quaternion slerp
// ============================================================

void quat_slerp(const float* a, const float* b, float t, float* out) {
    // Compute dot product
    float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];

    // If dot is negative, negate one quaternion to take the short path
    float b_x = b[0];
    float b_y = b[1];
    float b_z = b[2];
    float b_w = b[3];
    if (dot < 0.0F) {
        b_x = -b_x;
        b_y = -b_y;
        b_z = -b_z;
        b_w = -b_w;
        dot = -dot;
    }

    // Clamp dot to [-1, 1] to avoid acos domain errors
    if (dot > 1.0F) dot = 1.0F;
    if (dot < -1.0F) dot = -1.0F;

    const float threshold = 0.9995F;
    if (dot > threshold) {
        // Quaternions are very close; use linear interpolation
        out[0] = a[0] + t * (b_x - a[0]);
        out[1] = a[1] + t * (b_y - a[1]);
        out[2] = a[2] + t * (b_z - a[2]);
        out[3] = a[3] + t * (b_w - a[3]);
        // Normalize
        float len = std::sqrt(out[0] * out[0] + out[1] * out[1] +
                              out[2] * out[2] + out[3] * out[3]);
        if (len > 0.0F) {
            out[0] /= len;
            out[1] /= len;
            out[2] /= len;
            out[3] /= len;
        }
        return;
    }

    float theta_0 = std::acos(dot);        // angle between a and b
    float theta = theta_0 * t;             // angle between a and result
    float sin_theta = std::sin(theta);
    float sin_theta_0 = std::sin(theta_0);

    float s0 = std::cos(theta) - dot * sin_theta / sin_theta_0;
    float s1 = sin_theta / sin_theta_0;

    out[0] = s0 * a[0] + s1 * b_x;
    out[1] = s0 * a[1] + s1 * b_y;
    out[2] = s0 * a[2] + s1 * b_z;
    out[3] = s0 * a[3] + s1 * b_w;
}

// ============================================================
// Helper: find surrounding keyframes and interpolation factor
// ============================================================

namespace {

/**
 * Find the keyframe pair surrounding `time` and the blend factor in [0,1].
 * Returns false if there are no keyframes.
 */
bool find_keyframes(const std::vector<float>& times, float time,
                    std::size_t& idx0, std::size_t& idx1, float& t) {
    if (times.empty()) return false;

    if (time <= times.front()) {
        idx0 = 0;
        idx1 = 0;
        t = 0.0F;
        return true;
    }
    if (time >= times.back()) {
        idx0 = times.size() - 1;
        idx1 = times.size() - 1;
        t = 0.0F;
        return true;
    }

    // Find the first keyframe time > `time`
    auto it = std::upper_bound(times.begin(), times.end(), time);
    if (it == times.end()) {
        idx0 = times.size() - 1;
        idx1 = times.size() - 1;
        t = 0.0F;
        return true;
    }
    idx1 = static_cast<std::size_t>(it - times.begin());
    idx0 = (idx1 > 0) ? idx1 - 1 : 0;

    float t0 = times[idx0];
    float t1 = times[idx1];
    if (t1 > t0) {
        t = (time - t0) / (t1 - t0);
    } else {
        t = 0.0F;
    }
    return true;
}

} // namespace

// ============================================================
// Evaluate glTF animation
// ============================================================

void evaluate_animation(
    const AnimationClipData& anim,
    const Skin& skin,
    float time,
    std::vector<Mat4>& out_joint_matrices)
{
    const std::size_t joint_count = skin.joints.size();
    out_joint_matrices.resize(joint_count);

    // Per-joint local transforms: start as identity
    std::vector<Mat4> local_transforms(joint_count);
    for (std::size_t j = 0; j < joint_count; ++j) {
        local_transforms[j] = Mat4::identity();
    }

    // Build node_index → joint_index mapping
    std::unordered_map<int, int> node_to_joint;
    for (std::size_t j = 0; j < joint_count; ++j) {
        if (skin.joints[j].node_index >= 0) {
            node_to_joint[skin.joints[j].node_index] = static_cast<int>(j);
        }
    }

    // Process each animation channel
    for (const auto& channel : anim.channels) {
        if (channel.sampler_index < 0 ||
            static_cast<std::size_t>(channel.sampler_index) >= anim.samplers.size()) {
            continue;
        }

        // Map node index to joint index
        auto jt = node_to_joint.find(channel.node_index);
        if (jt == node_to_joint.end()) continue;
        int joint_idx = jt->second;

        const auto& sampler = anim.samplers[static_cast<std::size_t>(channel.sampler_index)];
        if (sampler.input_times.empty() || sampler.output_values.empty()) continue;

        // Find keyframe pair
        std::size_t kf0 = 0;
        std::size_t kf1 = 0;
        float blend = 0.0F;
        if (!find_keyframes(sampler.input_times, time, kf0, kf1, blend)) continue;

        if (channel.path == "translation") {
            // 3 floats per keyframe
            std::size_t stride = 3;
            std::size_t off0 = kf0 * stride;
            std::size_t off1 = kf1 * stride;
            if (off0 + 3 <= sampler.output_values.size() &&
                off1 + 3 <= sampler.output_values.size()) {
                float tx, ty, tz;
                if (sampler.interpolation == "STEP") {
                    tx = sampler.output_values[off0];
                    ty = sampler.output_values[off0 + 1];
                    tz = sampler.output_values[off0 + 2];
                } else {
                    tx = sampler.output_values[off0] +
                         blend * (sampler.output_values[off1] - sampler.output_values[off0]);
                    ty = sampler.output_values[off0 + 1] +
                         blend * (sampler.output_values[off1 + 1] - sampler.output_values[off0 + 1]);
                    tz = sampler.output_values[off0 + 2] +
                         blend * (sampler.output_values[off1 + 2] - sampler.output_values[off0 + 2]);
                }
                local_transforms[static_cast<std::size_t>(joint_idx)] =
                    local_transforms[static_cast<std::size_t>(joint_idx)] *
                    Mat4::translation(tx, ty, tz);
            }
        } else if (channel.path == "rotation") {
            // 4 floats per keyframe (quaternion)
            std::size_t stride = 4;
            std::size_t off0 = kf0 * stride;
            std::size_t off1 = kf1 * stride;
            if (off0 + 4 <= sampler.output_values.size() &&
                off1 + 4 <= sampler.output_values.size()) {
                float q[4];
                if (sampler.interpolation == "STEP") {
                    q[0] = sampler.output_values[off0];
                    q[1] = sampler.output_values[off0 + 1];
                    q[2] = sampler.output_values[off0 + 2];
                    q[3] = sampler.output_values[off0 + 3];
                } else {
                    quat_slerp(&sampler.output_values[off0],
                               &sampler.output_values[off1], blend, q);
                }
                local_transforms[static_cast<std::size_t>(joint_idx)] =
                    local_transforms[static_cast<std::size_t>(joint_idx)] *
                    Mat4::rotation_quat(q[0], q[1], q[2], q[3]);
            }
        } else if (channel.path == "scale") {
            // 3 floats per keyframe
            std::size_t stride = 3;
            std::size_t off0 = kf0 * stride;
            std::size_t off1 = kf1 * stride;
            if (off0 + 3 <= sampler.output_values.size() &&
                off1 + 3 <= sampler.output_values.size()) {
                float sx, sy, sz;
                if (sampler.interpolation == "STEP") {
                    sx = sampler.output_values[off0];
                    sy = sampler.output_values[off0 + 1];
                    sz = sampler.output_values[off0 + 2];
                } else {
                    sx = sampler.output_values[off0] +
                         blend * (sampler.output_values[off1] - sampler.output_values[off0]);
                    sy = sampler.output_values[off0 + 1] +
                         blend * (sampler.output_values[off1 + 1] - sampler.output_values[off0 + 1]);
                    sz = sampler.output_values[off0 + 2] +
                         blend * (sampler.output_values[off1 + 2] - sampler.output_values[off0 + 2]);
                }
                local_transforms[static_cast<std::size_t>(joint_idx)] =
                    local_transforms[static_cast<std::size_t>(joint_idx)] *
                    Mat4::scale(sx, sy, sz);
            }
        }
    }

    // Compute global transforms by traversing the hierarchy top-down.
    // Joints are assumed to be stored in topological order (parents before children),
    // which is the standard glTF convention.
    std::vector<Mat4> global_transforms(joint_count);
    for (std::size_t j = 0; j < joint_count; ++j) {
        const auto& joint = skin.joints[j];
        Mat4 local = local_transforms[j];
        if (joint.parent_index >= 0 &&
            static_cast<std::size_t>(joint.parent_index) < joint_count) {
            global_transforms[j] = global_transforms[static_cast<std::size_t>(joint.parent_index)] * local;
        } else {
            global_transforms[j] = local;
        }
    }

    // Final joint matrix = global_transform * inverse_bind_matrix
    for (std::size_t j = 0; j < joint_count; ++j) {
        const auto& joint = skin.joints[j];
        out_joint_matrices[j] = global_transforms[j] * joint.inverse_bind_matrix;
    }
}

// ============================================================
// Procedural idle animation
// ============================================================

void evaluate_procedural_animation(
    ProceduralAnimState& state,
    float dt,
    std::vector<Mat4>& out_joint_matrices)
{
    state.time += dt;

    // 8 joints: [0] root, [1] hips, [2] spine, [3] head,
    //           [4] left_arm, [5] right_arm, [6] left_leg, [7] right_leg
    constexpr std::size_t kNumJoints = 8;
    out_joint_matrices.resize(kNumJoints);

    float t = state.time;

    // Breathing rhythm: slow sine wave (period ~3 seconds)
    float breath = std::sin(t * 2.0F * 3.14159265F / 3.0F);
    // Subtle sway: slower, out of phase
    float sway = std::sin(t * 2.0F * 3.14159265F / 5.0F + 1.0F);

    // Joint 0: Root — identity
    Mat4 root = Mat4::identity();

    // Joint 1: Hips — subtle vertical bounce from breathing
    Mat4 hips = Mat4::translation(0.0F, breath * 0.015F, 0.0F);

    // Joint 2: Spine — slight forward/back rotation (nodding) + vertical
    float spine_rot_x = breath * 0.02F; // ~1.1 degrees
    Mat4 spine = Mat4::translation(0.0F, breath * 0.01F, sway * 0.005F) *
                 Mat4::rotation_quat(std::sin(spine_rot_x * 0.5F), 0.0F, 0.0F,
                                     std::cos(spine_rot_x * 0.5F));

    // Joint 3: Head — subtle nodding
    float head_nod = breath * 0.04F; // ~2.3 degrees
    Mat4 head = Mat4::translation(0.0F, 0.0F, 0.0F) *
                Mat4::rotation_quat(std::sin(head_nod * 0.5F), 0.0F, 0.0F,
                                    std::cos(head_nod * 0.5F));

    // Joint 4: Left arm — very subtle swing
    float arm_swing = sway * 0.03F;
    Mat4 left_arm = Mat4::rotation_quat(0.0F, 0.0F, std::sin(arm_swing * 0.5F),
                                         std::cos(arm_swing * 0.5F));

    // Joint 5: Right arm — opposite phase
    Mat4 right_arm = Mat4::rotation_quat(0.0F, 0.0F, std::sin(-arm_swing * 0.5F),
                                          std::cos(-arm_swing * 0.5F));

    // Joint 6: Left leg — practically static (subtle knee bend)
    float leg_bend = breath * 0.01F;
    Mat4 left_leg = Mat4::rotation_quat(std::sin(leg_bend * 0.5F), 0.0F, 0.0F,
                                         std::cos(leg_bend * 0.5F));

    // Joint 7: Right leg — same
    Mat4 right_leg = Mat4::rotation_quat(std::sin(leg_bend * 0.5F), 0.0F, 0.0F,
                                          std::cos(leg_bend * 0.5F));

    // Compute global transforms. Hierarchy:
    //   root → hips → spine → head
    //   root → hips → left_arm
    //   root → hips → right_arm
    //   root → hips → left_leg
    //   root → hips → right_leg
    Mat4 g_root     = root;
    Mat4 g_hips     = g_root * hips;
    Mat4 g_spine    = g_hips * spine;
    Mat4 g_head     = g_spine * head;
    Mat4 g_left_arm = g_hips * left_arm;
    Mat4 g_right_arm = g_hips * right_arm;
    Mat4 g_left_leg  = g_hips * left_leg;
    Mat4 g_right_leg = g_hips * right_leg;

    out_joint_matrices[0] = g_root;
    out_joint_matrices[1] = g_hips;
    out_joint_matrices[2] = g_spine;
    out_joint_matrices[3] = g_head;
    out_joint_matrices[4] = g_left_arm;
    out_joint_matrices[5] = g_right_arm;
    out_joint_matrices[6] = g_left_leg;
    out_joint_matrices[7] = g_right_leg;
}



PosePalette extract_pose_palette(const std::vector<Mat4>& pose) {
    PosePalette out;
    out.joint_count = static_cast<int>(pose.size());
    out.joint_matrices.resize(static_cast<size_t>(out.joint_count) * 16U);
    for (int i = 0; i < out.joint_count; ++i) {
        const auto& mat = pose[static_cast<size_t>(i)];
        for (int e = 0; e < 16; ++e) {
            out.joint_matrices[static_cast<size_t>(i) * 16U + static_cast<size_t>(e)] = mat.m[static_cast<size_t>(e)];
        }
    }
    return out;
}

} // namespace ae::skeleton
