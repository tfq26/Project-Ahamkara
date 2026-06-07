#include "ae/animation/ik.h"

#include <algorithm>
#include <cmath>

namespace ae::animation {

int IKSolver::add_chain(const IKChain& chain) {
    int idx = static_cast<int>(chains_.size());
    chains_.push_back(chain);
    targets_.emplace_back();  // disabled by default
    return idx;
}

void IKSolver::set_target(int chain_index, const IKTarget& target) {
    if (chain_index >= 0 && static_cast<std::size_t>(chain_index) < targets_.size()) {
        targets_[static_cast<std::size_t>(chain_index)] = target;
    }
}

IKSolveResult IKSolver::solve_two_bone(
    const IKChain& chain,
    const IKTarget& target,
    const render::Mat4& /* root_global_world */) {
    IKSolveResult result;
    result.converged = false;

    if (!target.enabled || target.weight <= 0.0F) {
        return result;
    }

    // Analytical two-bone IK solver.
    // Given:
    //   - Upper bone length L1, Lower bone length L2
    //   - Root joint position (in root_global_world)
    //   - Target world position
    //   - Pole vector (optional, for elbow/knee direction)
    //
    // Steps:
    //   1. Compute target direction vector from root to target
    //   2. If distance > L1 + L2, fully extend (not reachable)
    //   3. If distance < |L1 - L2|, fold completely (target too close)
    //   4. Use law of cosines to find elbow angle
    //   5. Compute shoulder rotation to aim at target
    //   6. Apply pole vector to determine elbow plane

    float L1 = chain.bone_length_upper;
    float L2 = chain.bone_length_lower;

    if (L1 <= 0.0F || L2 <= 0.0F) {
        return result;
    }

    float tx = target.target_x;
    float ty = target.target_y;
    float tz = target.target_z;

    // Distance from root to target
    float dist = std::sqrt(tx * tx + ty * ty + tz * tz);
    if (dist < 0.0001F) {
        return result;
    }

    bool reachable = (dist <= L1 + L2);

    // Clamp to reachable range
    float max_reach = L1 + L2;
    float min_reach = std::abs(L1 - L2);
    dist = std::clamp(dist, min_reach, max_reach);

    // Law of cosines: angle at elbow
    // cos(C) = (L1² + L2² - dist²) / (2 * L1 * L2)
    float cos_elbow = (L1 * L1 + L2 * L2 - dist * dist) / (2.0F * L1 * L2);
    cos_elbow = std::clamp(cos_elbow, -1.0F, 1.0F);
    float elbow_angle = std::acos(cos_elbow);

    // The mid joint bends by (π - elbow_angle) from straight
    float mid_bend_angle = 3.14159265F - elbow_angle;

    // Direction from root to target (normalized)
    float dir_x = tx / dist;
    float dir_y = ty / dist;
    float dir_z = tz / dist;

    // Root rotation: aim the upper bone toward the target,
    // then "over-rotate" by the angle from root-to-elbow
    // angle at root: law of cosines
    // cos(A) = (L1² + dist² - L2²) / (2 * L1 * dist)
    float cos_root = (L1 * L1 + dist * dist - L2 * L2) / (2.0F * L1 * dist);
    cos_root = std::clamp(cos_root, -1.0F, 1.0F);
    float root_angle = std::acos(cos_root);

    // Build root correction as a quaternion:
    // We rotate from the default pose direction (e.g., -Y for arm hanging down)
    // toward the target direction.
    // Default upper bone direction: negative Y (for arms hanging down) or
    // negative Y for legs. We compute the rotation from -Y to the target direction.
    //
    // Simplified: create an axis-angle rotation.
    // axis = cross(default_dir, target_dir), angle = acos(dot(default_dir, target_dir))

    float default_dir_x = 0.0F, default_dir_y = -1.0F, default_dir_z = 0.0F; // arm hanging down

    // dot product between default direction and target direction
    float dot_dir = default_dir_x * dir_x + default_dir_y * dir_y + default_dir_z * dir_z;
    dot_dir = std::clamp(dot_dir, -1.0F, 1.0F);
    float aim_angle = std::acos(dot_dir);

    // axis = cross(default_dir, target_dir)
    float axis_x = default_dir_y * dir_z - default_dir_z * dir_y;
    float axis_y = default_dir_z * dir_x - default_dir_x * dir_z;
    float axis_z = default_dir_x * dir_y - default_dir_y * dir_x;
    float axis_len = std::sqrt(axis_x * axis_x + axis_y * axis_y + axis_z * axis_z);

    if (axis_len > 0.0001F) {
        axis_x /= axis_len;
        axis_y /= axis_len;
        axis_z /= axis_len;
    } else {
        // Directions are parallel or anti-parallel
        axis_x = 0.0F; axis_y = 0.0F; axis_z = 1.0F;
    }

    // Apply the aim rotation at the root
    float half_aim = (aim_angle - root_angle) * 0.5F;
    float sin_half = std::sin(half_aim);

    result.root_correction.qx = axis_x * sin_half;
    result.root_correction.qy = axis_y * sin_half;
    result.root_correction.qz = axis_z * sin_half;
    result.root_correction.qw = std::cos(half_aim);

    // Mid joint correction: bend by mid_bend_angle around a perpendicular axis
    // The bend axis is perpendicular to the bone plane (typically the pole vector axis)
    // For a simple solver, bend around Z axis (for arms in XY plane)
    float half_mid = mid_bend_angle * 0.5F;
    result.mid_correction.qx = 0.0F;
    result.mid_correction.qy = 0.0F;
    result.mid_correction.qz = std::sin(half_mid);
    result.mid_correction.qw = std::cos(half_mid);

    result.converged = reachable;

    // Apply target weight
    if (target.weight < 1.0F) {
        JointTransform identity = JointTransform::identity();
        result.root_correction = JointTransform::blend(identity, result.root_correction, target.weight);
        result.mid_correction = JointTransform::blend(identity, result.mid_correction, target.weight);
    }

    return result;
}

}  // namespace ae::animation
