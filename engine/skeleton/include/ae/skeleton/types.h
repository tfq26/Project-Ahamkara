#pragma once

#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace ae::skeleton {

/**
 * @brief Column-major 4x4 transform matrix used by pose evaluation and skinning.
 *
 * Owned by the skeleton module so animation and render can share a neutral
 * math/pose contract without depending on each other.
 */
struct Mat4 {
    std::array<float, 16> m{};

    Mat4();
    static Mat4 identity();
    static Mat4 translation(float x, float y, float z);
    static Mat4 rotation_quat(float x, float y, float z, float w);
    static Mat4 scale(float x, float y, float z);
    Mat4 operator*(const Mat4& other) const;
};

/**
 * @brief Neutral joint definition (bind pose + inverse bind).
 */
struct Joint {
    std::string name;
    int node_index {-1};   // original graph node index (optional)
    int parent_index {-1};
    Mat4 inverse_bind_matrix;
};

/**
 * @brief Neutral skin: ordered joints for palette generation.
 */
struct Skin {
    std::vector<Joint> joints;
};

/**
 * @brief Animation channel targeting a joint property.
 */
struct AnimationChannel {
    int node_index {-1};
    std::string path; // "translation", "rotation", "scale"
    int sampler_index {-1};
};

/**
 * @brief Keyframe sampler for a joint property.
 */
struct AnimationSampler {
    std::vector<float> input_times;
    std::vector<float> output_values;
    std::string interpolation; // "LINEAR" or "STEP"
};

/**
 * @brief Named clip data independent of renderer/GLTF loader.
 */
struct AnimationClipData {
    std::string name;
    std::vector<AnimationChannel> channels;
    std::vector<AnimationSampler> samplers;
};

/**
 * @brief Spherical linear interpolation of unit quaternions.
 */
void quat_slerp(const float* a, const float* b, float t, float* out);

/**
 * @brief Evaluate a clip against a skin into joint matrices (skinning palette).
 */
void evaluate_animation(const AnimationClipData& animation,
                        const Skin& skin,
                        float time,
                        std::vector<Mat4>& out_joint_matrices);

/**
 * @brief Procedural humanoid pose generator (8 matrices: root + 7 joints).
 */
struct ProceduralAnimState {
    float time {0.0F};
};

void evaluate_procedural_animation(ProceduralAnimState& state,
                                   float dt,
                                   std::vector<Mat4>& out_joint_matrices);

/**
 * @brief Flatten joint matrices into a GPU-friendly float array (column-major).
 */
struct PosePalette {
    std::vector<float> joint_matrices;
    int joint_count {0};
};

PosePalette extract_pose_palette(const std::vector<Mat4>& pose);

} // namespace ae::skeleton
