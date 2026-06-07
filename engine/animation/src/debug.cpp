#include "ae/animation/debug.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace ae::animation {

void AnimationDebugger::build_overlay(const StateMachine& sm,
                                       const AnimationGraph& /*graph*/,
                                       AnimationDebugOverlay& out_overlay) {
    out_overlay.lines.clear();

    // Current state
    {
        AnimDebugLine line;
        char buf[128];
        std::snprintf(buf, sizeof(buf), "State: %s", sm.current_state_id().c_str());
        line.text = buf;
        line.r = 0.2F; line.g = 1.0F; line.b = 0.2F;
        out_overlay.lines.push_back(line);
    }

    // Transition info
    if (sm.is_transitioning()) {
        AnimDebugLine line;
        line.text = "  [transitioning]";
        line.r = 1.0F; line.g = 0.8F; line.b = 0.2F;
        out_overlay.lines.push_back(line);
    }

    // Active clips
    {
        AnimDebugLine header;
        header.text = "Active clips:";
        header.r = 0.6F; header.g = 0.6F; header.b = 0.6F;
        out_overlay.lines.push_back(header);

        for (const auto& ac : sm.active_clips()) {
            AnimDebugLine line;
            char buf[128];
            std::snprintf(buf, sizeof(buf), "  %s  w=%.2f  t=%.2f",
                         ac.clip_name.c_str(), ac.weight, ac.normalized_time);
            line.text = buf;
            line.r = 0.8F; line.g = 0.8F; line.b = 1.0F;
            out_overlay.lines.push_back(line);
        }
    }
}

void AnimationDebugger::log_trigger(const std::string& trigger_name) {
    log_.push_back("[trigger] " + trigger_name);
    if (log_.size() > static_cast<std::size_t>(kMaxLogEntries)) {
        log_.erase(log_.begin());
    }
}

void AnimationDebugger::log_state_change(const AnimStateId& from,
                                          const AnimStateId& to,
                                          const std::string& trigger) {
    std::string entry = from + " -> " + to;
    if (!trigger.empty()) {
        entry += " (" + trigger + ")";
    }
    log_.push_back(entry);
    if (log_.size() > static_cast<std::size_t>(kMaxLogEntries)) {
        log_.erase(log_.begin());
    }
}

void AnimationDebugger::extract_skeleton_lines(
    const std::vector<render::Mat4>& global_matrices,
    const std::vector<int>& parent_indices,
    const std::vector<std::string>& /*joint_names*/,
    std::vector<JointLine>& out_lines) {
    out_lines.clear();

    const std::size_t count = global_matrices.size();
    if (count == 0) return;

    for (std::size_t j = 0; j < count; ++j) {
        int parent = (j < parent_indices.size()) ? parent_indices[j] : -1;
        if (parent < 0 || static_cast<std::size_t>(parent) >= count) continue;

        // Extract translation from the 4x4 matrix (column-major, col 3)
        const auto& m_child = global_matrices[j];
        const auto& m_parent = global_matrices[static_cast<std::size_t>(parent)];

        JointLine line;
        line.x0 = m_parent.m[12];  // col 3, row 0
        line.y0 = m_parent.m[13];  // col 3, row 1
        line.z0 = m_parent.m[14];  // col 3, row 2
        line.x1 = m_child.m[12];
        line.y1 = m_child.m[13];
        line.z1 = m_child.m[14];

        // Color: green for bones, yellow for tips
        line.r = 0.0F;
        line.g = 1.0F;
        line.b = 0.0F;

        out_lines.push_back(line);
    }
}

}  // namespace ae::animation
