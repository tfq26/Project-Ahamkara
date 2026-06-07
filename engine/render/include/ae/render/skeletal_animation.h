#pragma once

#include "ae/render/gltf_loader.h"
#include <array>
#include <cmath>
#include <vector>

namespace ae::render {

/**
 * @brief 4x4 column-major matrix stored as 16 floats.
 *
 * Element at column c, row r is at index m[c*4 + r].
 * Column-major is the native layout for OpenGL and glTF.
 */
struct Mat4 {
    std::array<float, 16> m{}; // m[col*4 + row]

    Mat4();

    static Mat4 identity();
    static Mat4 translation(float x, float y, float z);
    static Mat4 rotation_quat(float x, float y, float z, float w);
    static Mat4 scale(float sx, float sy, float sz);

    Mat4 operator*(const Mat4& other) const;
};

/**
 * @brief Standard quaternion spherical linear interpolation.
 *
 * @param a  Source quaternion (4 floats: x, y, z, w).
 * @param b  Destination quaternion (4 floats: x, y, z, w).
 * @param t  Interpolation factor in [0, 1].
 * @param out Result quaternion (4 floats: x, y, z, w).
 */
void quat_slerp(const float* a, const float* b, float t, float* out);

/**
 * @brief Evaluate a glTF animation at a given time, producing joint matrices.
 *
 * The resulting matrices transform vertices from bind pose to animated pose
 * in model space: skinned_vertex = sum over joints of (weight * joint_matrix * bind_vertex).
 *
 * @param anim            The animation data (channels + samplers).
 * @param skin            The skin data (joint hierarchy + inverse bind matrices).
 * @param time            Animation time in seconds (wraps/clamps within keyframe range).
 * @param out_joint_matrices  Output: one Mat4 per joint, in joint index order.
 */
void evaluate_animation(
    const GltfAnimation& anim,
    const GltfSkin& skin,
    float time,
    std::vector<Mat4>& out_joint_matrices);

/**
 * @brief State for the procedural idle animation.
 */
struct ProceduralAnimState {
    float time = 0.0F;
};

/**
 * @brief Evaluate a simple procedural idle breathing/sway animation.
 *
 * Generates joint matrices for a 7-joint humanoid skeleton:
 *   [0] root (identity), [1] hips, [2] spine, [3] head,
 *   [4] left_arm, [5] right_arm, [6] left_leg, [7] right_leg
 *
 * @param state             Animation state (advance time with dt).
 * @param dt                Delta time in seconds since last evaluation.
 * @param out_joint_matrices  Output: 8 Mat4 matrices (root + 7 joints).
 */
void evaluate_procedural_animation(
    ProceduralAnimState& state,
    float dt,
    std::vector<Mat4>& out_joint_matrices);

} // namespace ae::render
