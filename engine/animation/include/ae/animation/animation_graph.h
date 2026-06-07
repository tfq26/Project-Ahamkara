#pragma once

#include "ae/animation/types.h"
#include "ae/animation/state_machine.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace ae::animation {

// ============================================================
// Animation Graph
//
// Manages the runtime evaluation of an animation graph:
//   - Maintains a pool of active clip instances
//   - Evaluates clips from a state machine's active clip list
//   - Blends multiple clips together into a single pose
//   - Applies IK corrections on top
//
// This is the "evaluation engine" — it takes the high-level
// state machine output and produces joint matrices for rendering.
// ============================================================

class AnimationGraph {
public:
    AnimationGraph() = default;

    // --- Asset registration ---

    /// Register an animation clip. The `source` pointer must remain valid
    /// for the lifetime of the graph (typically points into a GltfModel).
    void register_clip(const std::string& name, const render::GltfAnimation* source,
                       float duration, bool looping = true);

    /// Get a registered clip by name.
    [[nodiscard]] const AnimationClip* get_clip(const std::string& name) const;

    // --- Skinning setup ---

    /// Set the skin to use for skeletal evaluation.
    void set_skin(const render::GltfSkin* skin);

    /// Set the parent indices for global transform computation.
    void set_parent_indices(const std::vector<int>& parent_indices);

    // --- Runtime evaluation ---

    /// Main evaluate call. Takes a list of active clips with weights
    /// and produces blended joint matrices.
    ///
    /// During transitions (two clips with weights summing to 1.0),
    /// blends between them using per-joint slerp/lerp.
    ///
    /// @param active_clips List of (clip_name, weight, normalized_time, speed)
    /// @param dt Delta time (for advancing internal clip instances).
    /// @param out_matrices Output: one Mat4 per joint for GPU skinning.
    void evaluate(const std::vector<StateMachine::ActiveClip>& active_clips,
                  float dt,
                  std::vector<render::Mat4>& out_matrices);

    /// Evaluate a single clip directly (no blending).
    void evaluate_single(const std::string& clip_name, float dt,
                         std::vector<render::Mat4>& out_matrices);

    // --- Additives & IK ---

    /// Add an additive pose on top of the base pose.
    /// The additive transforms are joint-local offsets.
    void apply_additive(const std::vector<JointTransform>& additive_offsets,
                        std::vector<render::Mat4>& in_out_matrices,
                        const std::vector<int>& parent_indices);

    // --- Procedural ---

    /// Apply a procedural idle animation (breathing/sway) as a fallback.
    void evaluate_procedural(render::ProceduralAnimState& state, float dt,
                             std::vector<render::Mat4>& out_matrices);

private:
    /// Get or create a ClipInstance for the given clip name.
    ClipInstance& get_or_create_instance(const std::string& clip_name);

    /// Evaluate a single clip into a local pose.
    void evaluate_clip_to_pose(const std::string& clip_name,
                               float normalized_time,
                               AnimationPose& out_pose);

    std::unordered_map<std::string, AnimationClip> clips_;
    std::unordered_map<std::string, ClipInstance> instances_;
    const render::GltfSkin* skin_ {nullptr};
    std::vector<int> parent_indices_;
};

}  // namespace ae::animation
