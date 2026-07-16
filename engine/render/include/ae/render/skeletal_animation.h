#pragma once

/**
 * @file skeletal_animation.h
 * @brief Compatibility facade over ae::skeleton pose evaluation.
 *
 * Pose math and clip evaluation live in ae_skeleton. This header keeps existing
 * ae::render call sites compiling while the renderer depends one-way on
 * skeleton (never the reverse).
 */

#include "ae/skeleton/types.h"
#include "ae/render/gltf_loader.h"

#include <vector>

namespace ae::render {

using Mat4 = ae::skeleton::Mat4;
using ProceduralAnimState = ae::skeleton::ProceduralAnimState;

inline void quat_slerp(const float* a, const float* b, float t, float* out) {
    ae::skeleton::quat_slerp(a, b, t, out);
}

inline ae::skeleton::Skin to_skeleton_skin(const GltfSkin& skin) {
    ae::skeleton::Skin out;
    out.joints.reserve(skin.joints.size());
    for (const auto& joint : skin.joints) {
        ae::skeleton::Joint j;
        j.name = joint.name;
        j.node_index = joint.node_index;
        j.parent_index = joint.parent_index;
        j.inverse_bind_matrix = ae::skeleton::Mat4::identity();
        if (joint.inverse_bind_matrix.size() >= 16U) {
            for (int e = 0; e < 16; ++e) {
                j.inverse_bind_matrix.m[static_cast<size_t>(e)] = joint.inverse_bind_matrix[static_cast<size_t>(e)];
            }
        }
        out.joints.push_back(std::move(j));
    }
    return out;
}

inline ae::skeleton::AnimationClipData to_skeleton_clip(const GltfAnimation& animation) {
    ae::skeleton::AnimationClipData out;
    out.name = animation.name;
    out.channels.reserve(animation.channels.size());
    for (const auto& channel : animation.channels) {
        ae::skeleton::AnimationChannel c;
        c.node_index = channel.node_index;
        c.path = channel.path;
        c.sampler_index = channel.sampler_index;
        out.channels.push_back(std::move(c));
    }
    out.samplers.reserve(animation.samplers.size());
    for (const auto& sampler : animation.samplers) {
        ae::skeleton::AnimationSampler s;
        s.input_times = sampler.input_times;
        s.output_values = sampler.output_values;
        s.interpolation = sampler.interpolation;
        out.samplers.push_back(std::move(s));
    }
    return out;
}

inline void evaluate_animation(const GltfAnimation& animation,
                               const GltfSkin& skin,
                               float time,
                               std::vector<Mat4>& out_joint_matrices) {
    const auto clip = to_skeleton_clip(animation);
    const auto sk = to_skeleton_skin(skin);
    ae::skeleton::evaluate_animation(clip, sk, time, out_joint_matrices);
}

inline void evaluate_procedural_animation(ProceduralAnimState& state,
                                          float dt,
                                          std::vector<Mat4>& out_joint_matrices) {
    ae::skeleton::evaluate_procedural_animation(state, dt, out_joint_matrices);
}

} // namespace ae::render
