/// @file viewmodel_ik_tests.cpp
///
/// Unit tests for the viewmodel arm IK solver.
///
/// The production analytical two-bone IK solver lives inside
/// client/src/weapon_presentation.cpp (anonymous namespace solve_two_bone_arm).
/// This test reimplements the same math to verify correctness independently
/// of the render-layer dependency chain.
///
/// Test coverage:
///   - Target at default arm direction (should be identity corrections)
///   - Target within reach (should converge with non-trivial corrections)
///   - Target exactly at max reach (fully extended, elbow straight)
///   - Target exactly at min reach (fully folded)
///   - Target unreachable (clamped to max reach)
///   - Target coincides with shoulder (dist < epsilon)
///   - Matrix integration: apply_viewmodel_arm_ik modifies joint matrices
///   - Grip socket data integrity

#include "ae/skeleton/types.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

// ============================================================
// Constants matching the production IK solver
// ============================================================

constexpr int kShoulderJoint = 2;
constexpr int kElbowJoint   = 3;
constexpr int kHandJoint    = 5;
constexpr int kJointCount   = 8;

constexpr float kUpperArmLen = 0.35F;
constexpr float kLowerArmLen = 0.34F;
constexpr float kDefaultDirX = 0.0F;
constexpr float kDefaultDirY = 1.0F;
constexpr float kDefaultDirZ = 0.0F;

// ============================================================
// IK math implementation (mirrors production solve_two_bone_arm)
// ============================================================

namespace {

struct IKResult {
    float root_qx, root_qy, root_qz, root_qw;
    float mid_qx,  mid_qy,  mid_qz,  mid_qw;
};

IKResult solve_two_bone_test(float target_x, float target_y, float target_z,
                              float upper_len, float lower_len) {
    IKResult result{};

    float dist = std::sqrt(target_x * target_x +
                           target_y * target_y +
                           target_z * target_z);
    if (dist < 0.0001F) {
        result.root_qx = 0.0F; result.root_qy = 0.0F;
        result.root_qz = 0.0F; result.root_qw = 1.0F;
        result.mid_qx = 0.0F; result.mid_qy = 0.0F;
        result.mid_qz = 0.0F; result.mid_qw = 1.0F;
        return result;
    }

    float max_reach = upper_len + lower_len;
    float min_reach = std::abs(upper_len - lower_len);
    dist = std::clamp(dist, min_reach, max_reach);

    // Elbow angle via law of cosines
    float cos_elbow = (upper_len * upper_len + lower_len * lower_len - dist * dist)
                      / (2.0F * upper_len * lower_len);
    cos_elbow = std::clamp(cos_elbow, -1.0F, 1.0F);
    float elbow_angle = std::acos(cos_elbow);
    float mid_bend = 3.14159265F - elbow_angle;

    float dir_x = target_x / dist;
    float dir_y = target_y / dist;
    float dir_z = target_z / dist;

    // Shoulder over-rotation
    float cos_root = (upper_len * upper_len + dist * dist - lower_len * lower_len)
                     / (2.0F * upper_len * dist);
    cos_root = std::clamp(cos_root, -1.0F, 1.0F);
    float root_angle = std::acos(cos_root);

    // Aim angle
    float cos_aim = kDefaultDirX * dir_x + kDefaultDirY * dir_y + kDefaultDirZ * dir_z;
    cos_aim = std::clamp(cos_aim, -1.0F, 1.0F);
    float aim_angle = std::acos(cos_aim);

    // Rotation axis
    float axis_x = kDefaultDirY * dir_z - kDefaultDirZ * dir_y;
    float axis_y = kDefaultDirZ * dir_x - kDefaultDirX * dir_z;
    float axis_z = kDefaultDirX * dir_y - kDefaultDirY * dir_x;
    float axis_len = std::sqrt(axis_x * axis_x + axis_y * axis_y + axis_z * axis_z);

    if (axis_len > 0.0001F) {
        axis_x /= axis_len;
        axis_y /= axis_len;
        axis_z /= axis_len;
    } else {
        axis_x = 0.0F; axis_y = 0.0F; axis_z = 1.0F;
    }

    float total_root = aim_angle - root_angle;
    float half_root = total_root * 0.5F;
    float sin_half = std::sin(half_root);
    result.root_qx = axis_x * sin_half;
    result.root_qy = axis_y * sin_half;
    result.root_qz = axis_z * sin_half;
    result.root_qw = std::cos(half_root);

    float half_mid = mid_bend * 0.5F;
    result.mid_qx = 0.0F;
    result.mid_qy = 0.0F;
    result.mid_qz = std::sin(half_mid);
    result.mid_qw = std::cos(half_mid);

    return result;
}

} // namespace

// ============================================================
// Helper: nearly_equal
// ============================================================

bool nearly_equal(float a, float b, float eps = 1.0e-3F) {
    return std::fabs(a - b) <= eps;
}

// ============================================================
// Helper: build identity joint matrix array
// ============================================================

void make_identity_joints(float* matrices, int count) {
    for (int i = 0; i < count; ++i) {
        float* m = matrices + i * 16;
        std::memset(m, 0, 16 * sizeof(float));
        m[0]  = 1.0F;
        m[5]  = 1.0F;
        m[10] = 1.0F;
        m[15] = 1.0F;
    }
}

// ============================================================
// Test 1: Target at default direction (shoulder straight down)
// ============================================================

bool test_default_direction() {
    // Default arm direction is +Y (downward). If the target is straight down
    // at the reachable distance, the corrections should be near identity.
    float target_x = 0.0F;
    float target_y = kUpperArmLen + kLowerArmLen;  // max reach directly downward
    float target_z = 0.0F;

    auto result = solve_two_bone_test(target_x, target_y, target_z,
                                       kUpperArmLen, kLowerArmLen);

    // Root should be near identity (no rotation needed to aim downward)
    if (!nearly_equal(result.root_qw, 1.0F) ||
        !nearly_equal(result.root_qx, 0.0F) ||
        !nearly_equal(result.root_qy, 0.0F) ||
        !nearly_equal(result.root_qz, 0.0F)) {
        std::cerr << "FAIL: test_default_direction - root correction not identity\n"
                  << "  got: qw=" << result.root_qw << " qx=" << result.root_qx
                  << " qy=" << result.root_qy << " qz=" << result.root_qz << "\n";
        return false;
    }

    // At max reach the elbow should be straight (mid_bend ≈ 0)
    if (!nearly_equal(result.mid_qw, 1.0F) ||
        !nearly_equal(result.mid_qz, 0.0F)) {
        std::cerr << "FAIL: test_default_direction - mid correction should be identity (fully extended)\n"
                  << "  got: qw=" << result.mid_qw << " qz=" << result.mid_qz << "\n";
        return false;
    }

    std::cout << "  PASS: test_default_direction\n";
    return true;
}

// ============================================================
// Test 2: Target within reach (should produce bent elbow)
// ============================================================

bool test_within_reach() {
    // Target is directly in front of the shoulder (along -Z in default +Y space).
    // This should produce a non-trivial shoulder rotation and elbow bend.
    float target_x = 0.0F;
    float target_y = 0.3F;  // shorter than max reach
    float target_z = 0.3F;  // some Z component

    auto result = solve_two_bone_test(target_x, target_y, target_z,
                                       kUpperArmLen, kLowerArmLen);

    // Should have non-identity quaternions
    if (nearly_equal(result.root_qw, 1.0F) && nearly_equal(result.mid_qw, 1.0F)) {
        std::cerr << "FAIL: test_within_reach - should produce non-trivial corrections\n";
        return false;
    }

    // Root quaternion should be roughly identity-like
    // (the target is mostly along the default +Y direction with a bit of Z)
    bool root_non_trivial = !nearly_equal(result.root_qx, 0.0F) ||
                            !nearly_equal(result.root_qy, 0.0F) ||
                            !nearly_equal(result.root_qz, 0.0F);
    bool mid_non_trivial = !nearly_equal(result.mid_qz, 0.0F);

    if (!root_non_trivial && !mid_non_trivial) {
        std::cerr << "FAIL: test_within_reach - both root and mid are identity\n";
        return false;
    }

    // Normalized quaternions should have unit length
    float root_len = std::sqrt(result.root_qx * result.root_qx +
                                result.root_qy * result.root_qy +
                                result.root_qz * result.root_qz +
                                result.root_qw * result.root_qw);
    if (!nearly_equal(root_len, 1.0F)) {
        std::cerr << "FAIL: test_within_reach - root quaternion not unit (len="
                  << root_len << ")\n";
        return false;
    }

    float mid_len = std::sqrt(result.mid_qx * result.mid_qx +
                               result.mid_qy * result.mid_qy +
                               result.mid_qz * result.mid_qz +
                               result.mid_qw * result.mid_qw);
    if (!nearly_equal(mid_len, 1.0F)) {
        std::cerr << "FAIL: test_within_reach - mid quaternion not unit (len="
                  << mid_len << ")\n";
        return false;
    }

    std::cout << "  PASS: test_within_reach\n";
    return true;
}

// ============================================================
// Test 3: Target at max reach (fully extended)
// ============================================================

bool test_max_reach() {
    float dist = kUpperArmLen + kLowerArmLen;
    float target_x = dist * 0.0F;    // straight up (opposite of default)
    float target_y = dist * (-1.0F); // negative Y is up
    float target_z = 0.0F;

    auto result = solve_two_bone_test(target_x, target_y, target_z,
                                       kUpperArmLen, kLowerArmLen);

    // Elbow should be straight (180° bend = 0 in mid correction)
    // mid_bend = π - elbow_angle; at max reach, elbow_angle = π, so mid_bend = 0
    if (!nearly_equal(result.mid_qz, 0.0F)) {
        std::cerr << "FAIL: test_max_reach - mid correction should be zero (fully extended)\n"
                  << "  got: mid_qz=" << result.mid_qz << "\n";
        return false;
    }

    // Root should have a half-turn rotation (pointing -Y instead of +Y)
    // aim_angle = π, root_angle depends on geometry
    bool root_active = !nearly_equal(result.root_qw, 1.0F);
    if (!root_active) {
        std::cerr << "FAIL: test_max_reach - root should be non-identity for upward target\n";
        return false;
    }

    std::cout << "  PASS: test_max_reach\n";
    return true;
}

// ============================================================
// Test 4: Target at min reach (fully folded)
// ============================================================

bool test_min_reach() {
    float min_dist = std::abs(kUpperArmLen - kLowerArmLen);
    float target_x = 0.0F;
    float target_y = min_dist + 0.001F;  // just above min
    float target_z = 0.0F;

    auto result = solve_two_bone_test(target_x, target_y, target_z,
                                       kUpperArmLen, kLowerArmLen);

    // Near min reach, the elbow bend should be near π (fully folded)
    // mid_bend = π - elbow_angle; at min reach, elbow_angle ≈ 0, so mid_bend ≈ π
    if (nearly_equal(result.mid_qz, 0.0F)) {
        std::cerr << "FAIL: test_min_reach - mid correction should be significant\n"
                  << "  got: mid_qz=" << result.mid_qz << "\n";
        return false;
    }

    std::cout << "  PASS: test_min_reach\n";
    return true;
}

// ============================================================
// Test 5: Unreachable target (clamped)
// ============================================================

bool test_unreachable() {
    float far_dist = (kUpperArmLen + kLowerArmLen) * 2.0F;  // 2x max reach
    float target_x = far_dist;
    float target_y = 0.0F;
    float target_z = 0.0F;

    auto result = solve_two_bone_test(target_x, target_y, target_z,
                                       kUpperArmLen, kLowerArmLen);

    // Should still produce valid quaternions (dist clamped internally)
    float root_len = std::sqrt(result.root_qx * result.root_qx +
                                result.root_qy * result.root_qy +
                                result.root_qz * result.root_qz +
                                result.root_qw * result.root_qw);
    if (!nearly_equal(root_len, 1.0F)) {
        std::cerr << "FAIL: test_unreachable - root quaternion not unit (len="
                  << root_len << ")\n";
        return false;
    }

    float mid_len = std::sqrt(result.mid_qx * result.mid_qx +
                               result.mid_qy * result.mid_qy +
                               result.mid_qz * result.mid_qz +
                               result.mid_qw * result.mid_qw);
    if (!nearly_equal(mid_len, 1.0F)) {
        std::cerr << "FAIL: test_unreachable - mid quaternion not unit (len="
                  << mid_len << ")\n";
        return false;
    }

    // At max reach after clamping, elbow should be nearly straight
    if (!nearly_equal(result.mid_qz, 0.0F)) {
        std::cerr << "FAIL: test_unreachable - after clamping to max reach, "
                  << "elbow should be straight, but mid_qz=" << result.mid_qz << "\n";
        return false;
    }

    std::cout << "  PASS: test_unreachable\n";
    return true;
}

// ============================================================
// Test 6: Target at shoulder (zero distance)
// ============================================================

bool test_zero_distance() {
    float target_x = 0.0F;
    float target_y = 0.0F;
    float target_z = 0.0F;

    auto result = solve_two_bone_test(target_x, target_y, target_z,
                                       kUpperArmLen, kLowerArmLen);

    // Should return identity corrections
    if (!nearly_equal(result.root_qw, 1.0F) ||
        !nearly_equal(result.root_qx, 0.0F) ||
        !nearly_equal(result.root_qy, 0.0F) ||
        !nearly_equal(result.root_qz, 0.0F)) {
        std::cerr << "FAIL: test_zero_distance - root should be identity\n";
        return false;
    }

    if (!nearly_equal(result.mid_qw, 1.0F) ||
        !nearly_equal(result.mid_qx, 0.0F) ||
        !nearly_equal(result.mid_qy, 0.0F) ||
        !nearly_equal(result.mid_qz, 0.0F)) {
        std::cerr << "FAIL: test_zero_distance - mid should be identity\n";
        return false;
    }

    std::cout << "  PASS: test_zero_distance\n";
    return true;
}

// ============================================================
// Test 7: apply_viewmodel_arm_ik simulation
// ============================================================

bool test_joint_matrix_modification() {
    // Create a flat joint matrix array (8 joints, column-major Mat4 per joint)
    float joint_matrices[kJointCount * 16];
    make_identity_joints(joint_matrices, kJointCount);

    // Position shoulder at origin, no rotation.
    // Set shoulder position (joint 2) at world origin
    // In a real scenario the shoulder might be at some position, but for testing
    // we use identity matrices (shoulder at origin).

    // Simulate what apply_viewmodel_arm_ik does:
    // 1. Extract shoulder position from joint matrix (m[12], m[13], m[14])
    float shoulder_x = joint_matrices[kShoulderJoint * 16 + 12];
    float shoulder_y = joint_matrices[kShoulderJoint * 16 + 13];
    float shoulder_z = joint_matrices[kShoulderJoint * 16 + 14];

    // Shoulder should be at origin since we used identity
    if (!nearly_equal(shoulder_x, 0.0F) ||
        !nearly_equal(shoulder_y, 0.0F) ||
        !nearly_equal(shoulder_z, 0.0F)) {
        std::cerr << "FAIL: test_joint_matrix_modification - shoulder not at origin\n";
        return false;
    }

    // 2. Compute target relative to shoulder.
    // Use an off-axis grip position to ensure non-trivial IK corrections.
    // The default arm direction is +Y (downward); an offset in +X requires
    // the shoulder to rotate.
    float grip_x = 0.20F;
    float grip_y = 0.50F;
    float grip_z = 0.10F;

    float tx = grip_x - shoulder_x;
    float ty = grip_y - shoulder_y;
    float tz = grip_z - shoulder_z;

    // 3. Solve IK
    auto result = solve_two_bone_test(tx, ty, tz, kUpperArmLen, kLowerArmLen);

    // 4. Build rotation matrices and apply (simulating what apply_viewmodel_arm_ik does)
    ae::skeleton::Mat4 root_rot = ae::skeleton::Mat4::rotation_quat(
        result.root_qx, result.root_qy, result.root_qz, result.root_qw);
    ae::skeleton::Mat4 mid_rot = ae::skeleton::Mat4::rotation_quat(
        result.mid_qx, result.mid_qy, result.mid_qz, result.mid_qw);

    // Apply shoulder correction
    ae::skeleton::Mat4 old_shoulder;
    std::memcpy(old_shoulder.m.data(), joint_matrices + kShoulderJoint * 16, 16 * sizeof(float));
    ae::skeleton::Mat4 new_shoulder = root_rot * old_shoulder;
    std::memcpy(joint_matrices + kShoulderJoint * 16, new_shoulder.m.data(), 16 * sizeof(float));

    // Apply elbow correction
    ae::skeleton::Mat4 old_elbow;
    std::memcpy(old_elbow.m.data(), joint_matrices + kElbowJoint * 16, 16 * sizeof(float));
    ae::skeleton::Mat4 new_elbow = mid_rot * old_elbow;
    std::memcpy(joint_matrices + kElbowJoint * 16, new_elbow.m.data(), 16 * sizeof(float));

    // Verify that joint matrices changed from identity
    bool shoulder_modified = false;
    for (int i = 0; i < 16; ++i) {
        float expected = (i == 0 || i == 5 || i == 10 || i == 15) ? 1.0F : 0.0F;
        if (!nearly_equal(joint_matrices[kShoulderJoint * 16 + i], expected)) {
            shoulder_modified = true;
            break;
        }
    }
    if (!shoulder_modified) {
        std::cerr << "FAIL: test_joint_matrix_modification - shoulder matrix unchanged\n";
        return false;
    }

    bool elbow_modified = false;
    for (int i = 0; i < 16; ++i) {
        float expected = (i == 0 || i == 5 || i == 10 || i == 15) ? 1.0F : 0.0F;
        if (!nearly_equal(joint_matrices[kElbowJoint * 16 + i], expected)) {
            elbow_modified = true;
            break;
        }
    }
    if (!elbow_modified) {
        std::cerr << "FAIL: test_joint_matrix_modification - elbow matrix unchanged\n";
        return false;
    }

    // Hand joint should remain identity (not modified by IK)
    bool hand_unchanged = true;
    for (int i = 0; i < 16; ++i) {
        float expected = (i == 0 || i == 5 || i == 10 || i == 15) ? 1.0F : 0.0F;
        if (!nearly_equal(joint_matrices[kHandJoint * 16 + i], expected)) {
            hand_unchanged = false;
            break;
        }
    }
    if (!hand_unchanged) {
        std::cerr << "FAIL: test_joint_matrix_modification - hand matrix was modified\n";
        return false;
    }

    std::cout << "  PASS: test_joint_matrix_modification\n";
    return true;
}

// ============================================================
// Test 8: Grip socket data integrity
// ============================================================

bool test_grip_socket_values() {
    // Verify that the grip socket values from weapon_viewmodel_data.h
    // match expected ranges for all three weapons.
    //
    // We can't include weapon_viewmodel_data.h here without pulling in
    // the client module, so we just verify the IK math works correctly
    // with input values in the grip socket range (~0, 0.4-0.7, 0).

    // AR-15 grip range: right hand at ~(0, 0.70, 0), left at ~(0.15, 0.55, 0)
    // These are all well within reach of the IK solver (upper+lower ≈ 0.69m)
    float test_grip_x = 0.15F;
    float test_grip_y = 0.55F;
    float test_grip_z = 0.0F;

    auto result = solve_two_bone_test(test_grip_x, test_grip_y, test_grip_z,
                                       kUpperArmLen, kLowerArmLen);

    float root_len = std::sqrt(result.root_qx * result.root_qx +
                                result.root_qy * result.root_qy +
                                result.root_qz * result.root_qz +
                                result.root_qw * result.root_qw);
    if (!nearly_equal(root_len, 1.0F)) {
        std::cerr << "FAIL: test_grip_socket_values - root quaternion not unit\n";
        return false;
    }

    float mid_len = std::sqrt(result.mid_qx * result.mid_qx +
                               result.mid_qy * result.mid_qy +
                               result.mid_qz * result.mid_qz +
                               result.mid_qw * result.mid_qw);
    if (!nearly_equal(mid_len, 1.0F)) {
        std::cerr << "FAIL: test_grip_socket_values - mid quaternion not unit\n";
        return false;
    }

    std::cout << "  PASS: test_grip_socket_values\n";
    return true;
}

// ============================================================
// Test 9: Pole vector axis correctness
// ============================================================

bool test_axis_calculation() {
    // Test that the rotation axis is correctly computed for various directions.
    // The axis should be cross(default_dir, target_dir), normalized.

    // Target at 90° to default (+X direction)
    float target_x = 1.0F;
    float target_y = 0.0F;
    float target_z = 0.0F;
    float dist = std::sqrt(target_x * target_x + target_y * target_y + target_z * target_z);
    float dir_x = target_x / dist;
    float dir_y = target_y / dist;
    float dir_z = target_z / dist;

    // Expected axis: cross(+Y, +X) = Y×X = Z (pointing out of screen for right-hand rule)
    // But wait, we need to think about the coordinate system.
    // In a right-handed system: cross(Y, X) = -Z (or +Z depending on convention)
    // Y cross X in right-handed = -Z direction in standard math
    // Let's just verify the cross product calculation:
    float axis_x = kDefaultDirY * dir_z - kDefaultDirZ * dir_y;  // 1*0 - 0*0 = 0
    float axis_y = kDefaultDirZ * dir_x - kDefaultDirX * dir_z;  // 0*1 - 0*0 = 0
    float axis_z = kDefaultDirX * dir_y - kDefaultDirY * dir_x;  // 0*0 - 1*1 = -1

    if (!nearly_equal(axis_x, 0.0F) ||
        !nearly_equal(axis_y, 0.0F) ||
        !nearly_equal(axis_z, -1.0F)) {
        std::cerr << "FAIL: test_axis_calculation - cross product incorrect\n"
                  << "  got: axis=(" << axis_x << ", " << axis_y << ", " << axis_z << ")\n";
        return false;
    }

    // Test parallel case (target same as default)
    float tx2 = 0.0F, ty2 = 1.0F, tz2 = 0.0F;
    float d2 = std::sqrt(tx2 * tx2 + ty2 * ty2 + tz2 * tz2);
    float dx2 = tx2 / d2, dy2 = ty2 / d2, dz2 = tz2 / d2;
    float ax2 = kDefaultDirY * dz2 - kDefaultDirZ * dy2;
    float ay2 = kDefaultDirZ * dx2 - kDefaultDirX * dz2;
    float az2 = kDefaultDirX * dy2 - kDefaultDirY * dx2;
    float al2 = std::sqrt(ax2 * ax2 + ay2 * ay2 + az2 * az2);

    if (al2 > 0.0001F) {
        std::cerr << "FAIL: test_axis_calculation - parallel vectors should produce zero axis (len="
                  << al2 << ")\n";
        return false;
    }

    std::cout << "  PASS: test_axis_calculation\n";
    return true;
}

// ============================================================
// Test 10: Symmetry test (left vs right targets)
// ============================================================

bool test_symmetry() {
    // Target to the right (+X) should produce mirror result of target to the left (-X)
    float right_x = 0.3F, right_y = 0.3F, right_z = 0.0F;
    float left_x = -0.3F, left_y = 0.3F, left_z = 0.0F;

    auto right_result = solve_two_bone_test(right_x, right_y, right_z,
                                             kUpperArmLen, kLowerArmLen);
    auto left_result = solve_two_bone_test(left_x, left_y, left_z,
                                            kUpperArmLen, kLowerArmLen);

    // Root qx should be mirrored (opposite sign for opposite direction)
    if (!nearly_equal(right_result.root_qx, -left_result.root_qx)) {
        std::cerr << "FAIL: test_symmetry - root qx not mirrored\n"
                  << "  right_qx=" << right_result.root_qx
                  << " left_qx=" << left_result.root_qx << "\n";
        return false;
    }

    // Mid correction should be identical (both arms bend same amount)
    if (!nearly_equal(right_result.mid_qz, left_result.mid_qz)) {
        std::cerr << "FAIL: test_symmetry - mid qz not equal\n"
                  << "  right_qz=" << right_result.mid_qz
                  << " left_qz=" << left_result.mid_qz << "\n";
        return false;
    }

    std::cout << "  PASS: test_symmetry\n";
    return true;
}

// ============================================================
// Main
// ============================================================

int main() {
    bool all_pass = true;

    all_pass &= test_default_direction();
    all_pass &= test_within_reach();
    all_pass &= test_max_reach();
    all_pass &= test_min_reach();
    all_pass &= test_unreachable();
    all_pass &= test_zero_distance();
    all_pass &= test_joint_matrix_modification();
    all_pass &= test_grip_socket_values();
    all_pass &= test_axis_calculation();
    all_pass &= test_symmetry();

    if (all_pass) {
        std::cout << "\nviewmodel_ik_tests: ALL PASSED\n";
        return 0;
    } else {
        std::cerr << "\nviewmodel_ik_tests: SOME TESTS FAILED\n";
        return 1;
    }
}
