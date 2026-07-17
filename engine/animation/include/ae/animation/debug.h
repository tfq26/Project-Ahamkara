#pragma once

#include "ae/animation/types.h"
#include "ae/animation/state_machine.h"
#include "ae/animation/animation_graph.h"

#include <string>
#include <vector>

namespace ae::animation {

// ============================================================
// Animation Debugging Tools
//
// Provides introspection and visualization for the animation
// system. Useful during development to inspect state machines,
// blend values, clip times, and joint transforms.
//
// Not intended for shipping builds — gated by debug flags.
// ============================================================

/// A single line of debug text for overlay display
struct AnimDebugLine {
    std::string text;
    float r {1.0F}, g {1.0F}, b {1.0F};
};

/// Debug overlay for animation state
struct AnimationDebugOverlay {
    std::vector<AnimDebugLine> lines;
};

// ============================================================
// Animation Debugger
// ============================================================

class AnimationDebugger {
public:
    AnimationDebugger() = default;

    /// Whether the animation debug overlay is visible
    bool visible {false};

    /// Generate debug overlay lines from the current animation state.
    void build_overlay(const StateMachine& sm,
                       const AnimationGraph& graph,
                       AnimationDebugOverlay& out_overlay);

    /// Record a trigger event for logging
    void log_trigger(const std::string& trigger_name);

    /// Record a state change for logging
    void log_state_change(const AnimStateId& from, const AnimStateId& to,
                          const std::string& trigger);

    /// Draw joint transforms as lines (for debug rendering)
    /// @param global_matrices  World-space joint matrices.
    /// @param parent_indices   Parent indices per joint (-1 for root).
    /// @param joint_names      Optional: names for each joint.
    /// @param out_lines        Output: pairs of (start, end) world-space points.
    struct JointLine {
        float x0, y0, z0;
        float x1, y1, z1;
        float r {0.0F}, g {1.0F}, b {0.0F}; // green bones
    };
    void extract_skeleton_lines(const std::vector<skeleton::Mat4>& global_matrices,
                                const std::vector<int>& parent_indices,
                                const std::vector<std::string>& joint_names,
                                std::vector<JointLine>& out_lines);

    /// Get the recent log of triggers and state changes.
    [[nodiscard]] const std::vector<std::string>& recent_log() const { return log_; }

private:
    std::vector<std::string> log_;
    static constexpr int kMaxLogEntries = 64;
};

}  // namespace ae::animation
