#pragma once

#include "ae/animation/types.h"

#include <vector>

namespace ae::animation {

// ============================================================
// Inverse Kinematics — Hand/Foot Placement
//
// Two-bone IK solver for common FPS/TPS use cases:
//   - Foot placement on uneven terrain
//   - Hand placement on weapon grips
//   - Spine adjustment for aim direction
//
// The solver operates in joint-local space and returns
// modifications to be applied on top of animation output.
// ============================================================

/// A single IK target in world space
struct IKTarget {
    float target_x {0.0F}, target_y {0.0F}, target_z {0.0F}; // world position
    bool enabled {false};
    float weight {1.0F}; // 0..1 blend with original animation
};

/// Describes a two-bone IK chain for solving
struct IKChain {
    int root_joint {-1};      // e.g., shoulder / hip
    int mid_joint {-1};       // e.g., elbow / knee
    int end_joint {-1};       // e.g., wrist / ankle (end effector)
    int pole_joint {-1};      // optional: pole vector target joint
    float bone_length_upper {0.3F};  // root → mid
    float bone_length_lower {0.3F};  // mid → end
};

/// Result of a two-bone IK solve
struct IKSolveResult {
    JointTransform root_correction;   // rotation to apply at root joint
    JointTransform mid_correction;    // rotation to apply at mid joint
    bool converged {false};           // true if target was reachable
};

// ============================================================
// IK Solver — Hand/Foot Placement
//
// Plans for future implementation:
//   - Two-bone CCD or analytical IK solver
//   - Multiple chains: left_arm, right_arm, left_leg, right_leg, spine
//   - Foot IK: raycast down from hips to place feet on ground
//   - Hand IK: align hands to weapon attach points
//   - Look-at IK: adjust spine/neck to aim direction
// ============================================================

class IKSolver {
public:
    IKSolver() = default;

    /// Add an IK chain. Returns an index for later reference.
    int add_chain(const IKChain& chain);

    /// Set the world-space target for a chain.
    void set_target(int chain_index, const IKTarget& target);

    /// Solve a single two-bone IK chain analytically.
    /// @param chain The chain definition.
    /// @param target_world World-space target for the end effector.
    /// @param root_global_world Current global transform of the root joint.
    /// @return Correction transforms and convergence status.
    [[nodiscard]] static IKSolveResult solve_two_bone(
        const IKChain& chain,
        const IKTarget& target,
        const skeleton::Mat4& root_global_world);

    // --- Future API surface ---
    //
    // void solve_all(const std::vector<skeleton::Mat4>& global_matrices,
    //                std::vector<JointTransform>& out_corrections);
    //
    // Foot placement:
    //   struct FootPlacementResult { JointTransform ankle_correction; float ground_height; };
    //   FootPlacementResult solve_foot(int chain_index, float hip_height, float ground_y);
    //
    // Hand placement:
    //   void align_hand_to_point(int chain_index, const Vec3& world_point, const Vec3& world_normal);
    //
    // Spine aim:
    //   void aim_spine(const Vec3& aim_direction, float max_angle);

  private:
    std::vector<IKChain> chains_;
    std::vector<IKTarget> targets_;
};

}  // namespace ae::animation
