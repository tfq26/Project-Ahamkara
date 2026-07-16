#include "ae/core/log.h"
#include "ae/animation/animation_graph.h"

#include <algorithm>
#include <cmath>


#define AE_LOG_CATEGORY "Animation"

namespace ae::animation {

void AnimationGraph::register_clip(const std::string& name,
                                    const skeleton::AnimationClipData* source,
                                    float duration, bool looping) {
    AnimationClip clip;
    clip.name = name;
    clip.source = source;
    clip.duration_seconds = duration;
    clip.looping = looping;
    clips_[name] = clip;
}

const AnimationClip* AnimationGraph::get_clip(const std::string& name) const {
    auto it = clips_.find(name);
    if (it != clips_.end()) {
        return &it->second;
    }
    return nullptr;
}

void AnimationGraph::set_skin(const skeleton::Skin* skin) {
    skin_ = skin;
}

void AnimationGraph::set_parent_indices(const std::vector<int>& parent_indices) {
    parent_indices_ = parent_indices;
}

void AnimationGraph::evaluate(const std::vector<StateMachine::ActiveClip>& active_clips,
                               float dt,
                               std::vector<skeleton::Mat4>& out_matrices) {
    if (!skin_) {
        // No skin registered — fall back to procedural
        // (caller should provide a procedural state externally)
        out_matrices.clear();
        return;
    }

    const std::size_t joint_count = skin_->joints.size();
    if (joint_count == 0) {
        out_matrices.clear();
        return;
    }

    out_matrices.resize(joint_count);

    if (active_clips.empty()) {
        // No active clips — output identity
        for (std::size_t j = 0; j < joint_count; ++j) {
            out_matrices[j] = skeleton::Mat4::identity();
        }
        return;
    }

    // For each active clip, evaluate its source animation at the given time
    // and blend the resulting joint matrices.

    // Case 1: single clip — just evaluate directly
    if (active_clips.size() == 1) {
        const auto& ac = active_clips[0];
        auto* clip = get_clip(ac.clip_name);
        if (clip && clip->source) {
            float time = ac.normalized_time * clip->duration_seconds;
            skeleton::evaluate_animation(*clip->source, *skin_, time, out_matrices);
        } else {
            // Unknown clip — fall back to identity
            for (std::size_t j = 0; j < joint_count; ++j) {
                out_matrices[j] = skeleton::Mat4::identity();
            }
        }
        return;
    }

    // Case 2: multiple clips — evaluate each and blend
    // Collect poses for each clip
    struct ClipPose {
        std::vector<skeleton::Mat4> matrices;
        float weight;
    };
    std::vector<ClipPose> poses;
    poses.reserve(active_clips.size());

    for (const auto& ac : active_clips) {
        auto* clip = get_clip(ac.clip_name);
        if (!clip || !clip->source) continue;

        ClipPose cp;
        cp.weight = ac.weight;
        cp.matrices.resize(joint_count);
        float time = ac.normalized_time * clip->duration_seconds;
        skeleton::evaluate_animation(*clip->source, *skin_, time, cp.matrices);
        poses.push_back(std::move(cp));
    }

    if (poses.empty()) {
        for (std::size_t j = 0; j < joint_count; ++j) {
            out_matrices[j] = skeleton::Mat4::identity();
        }
        return;
    }

    if (poses.size() == 1) {
        out_matrices = std::move(poses[0].matrices);
        return;
    }

    // Blend: decompose each joint matrix into translation/rotation/scale,
    // blend using slerp/lerp, then recompose.
    // For N poses, we do N-1 pairwise blends weighted by sum.
    //
    // Simplified approach: normalize weights, then sequential blend.
    float total_weight = 0.0F;
    for (const auto& p : poses) total_weight += p.weight;
    if (total_weight <= 0.0001F) total_weight = 1.0F;

    // Start with the first pose
    for (std::size_t j = 0; j < joint_count; ++j) {
        out_matrices[j] = poses[0].matrices[j];
    }
    float accumulated_weight = poses[0].weight / total_weight;

    // Blend in remaining poses
    for (std::size_t p = 1; p < poses.size(); ++p) {
        float blend_t = 0.0F;
        float next_weight = poses[p].weight / total_weight;
        float new_accumulated = accumulated_weight + next_weight;
        if (new_accumulated > 0.0001F) {
            blend_t = next_weight / new_accumulated;
        }

        // Per-joint blend: decompose both Mat4, blend, recompose
        // For simplicity we perform a matrix lerp which is approximate but fast.
        // A proper decomposition would extract TRS from each 4x4.
        for (std::size_t j = 0; j < joint_count; ++j) {
            for (int i = 0; i < 16; ++i) {
                out_matrices[j].m[static_cast<std::size_t>(i)] =
                    out_matrices[j].m[static_cast<std::size_t>(i)] * (1.0F - blend_t) +
                    poses[p].matrices[j].m[static_cast<std::size_t>(i)] * blend_t;
            }
        }

        accumulated_weight = new_accumulated;
    }
}

void AnimationGraph::evaluate_single(const std::string& clip_name, float dt,
                                      std::vector<skeleton::Mat4>& out_matrices) {
    std::vector<StateMachine::ActiveClip> clips;
    StateMachine::ActiveClip ac;
    ac.clip_name = clip_name;
    ac.weight = 1.0F;
    ac.normalized_time = 0.0F;

    auto& inst = get_or_create_instance(clip_name);
    inst.active = true;
    inst.advance(dt);
    ac.normalized_time = inst.normalized_time();

    clips.push_back(ac);
    evaluate(clips, dt, out_matrices);
}

void AnimationGraph::apply_additive(const std::vector<JointTransform>& additive_offsets,
                                     std::vector<skeleton::Mat4>& in_out_matrices,
                                     const std::vector<int>& parent_indices) {
    const std::size_t count = std::min(additive_offsets.size(), in_out_matrices.size());
    if (count == 0) return;

    // Convert additive transforms to additive matrices and compose
    for (std::size_t j = 0; j < count; ++j) {
        skeleton::Mat4 additive = additive_offsets[j].to_mat4();
        // Apply additive: result = base * additive (in local space)
        in_out_matrices[j] = in_out_matrices[j] * additive;
    }

    // Recompute global transforms with parent hierarchy
    // (simplified: just chain multiplies)
    if (!parent_indices.empty()) {
        for (std::size_t j = 0; j < count; ++j) {
            int parent = (j < parent_indices.size()) ? parent_indices[j] : -1;
            if (parent >= 0 && static_cast<std::size_t>(parent) < count) {
                in_out_matrices[j] = in_out_matrices[static_cast<std::size_t>(parent)] *
                                     in_out_matrices[j];
            }
        }
    }
}

void AnimationGraph::evaluate_procedural(skeleton::ProceduralAnimState& state, float dt,
                                          std::vector<skeleton::Mat4>& out_matrices) {
    skeleton::evaluate_procedural_animation(state, dt, out_matrices);
}

ClipInstance& AnimationGraph::get_or_create_instance(const std::string& clip_name) {
    auto it = instances_.find(clip_name);
    if (it == instances_.end()) {
        ClipInstance inst;
        auto* clip = get_clip(clip_name);
        inst.clip = clip;
        inst.active = (clip != nullptr);
        inst.reset();
        auto [inserted_it, _] = instances_.emplace(clip_name, std::move(inst));
        return inserted_it->second;
    }
    return it->second;
}

void AnimationGraph::evaluate_clip_to_pose(const std::string& clip_name,
                                            float normalized_time,
                                            AnimationPose& out_pose) {
    auto* clip = get_clip(clip_name);
    if (!clip || !clip->source || !skin_) {
        return;
    }

    float time = normalized_time * clip->duration_seconds;
    skeleton::evaluate_animation(*clip->source, *skin_, time, out_pose.global_matrices);
}

}  // namespace ae::animation
