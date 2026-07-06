#include "ae/core/log.h"
#include "ahamkara/client/weapon_presentation.h"

#include "ahamkara/client/weapon_viewmodel_data.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#define AE_LOG_CATEGORY "Client"

namespace ahamkara::client {

void WeaponViewmodelPresentation::set_backend(ae::render::RenderBackend* backend) {
    cache_.set_backend(backend);
}

const ae::render::GpuModel* WeaponViewmodelPresentation::resolve_viewmodel(int weapon_index) {
    return cache_.get_gpu_model(ahamkara::client::weapon_viewmodel_mesh_path(weapon_index));
}

bool WeaponViewmodelPresentation::play_animation(int weapon_index, std::string_view clip_name) {
    return cache_.play_animation(ahamkara::client::weapon_viewmodel_mesh_path(weapon_index), clip_name, true);
}

const ae::render::Mat4* WeaponViewmodelPresentation::joint_matrices(int weapon_index) const {
    return cache_.current_joint_matrices(ahamkara::client::weapon_viewmodel_mesh_path(weapon_index));
}

int WeaponViewmodelPresentation::joint_count(int weapon_index) const {
    return cache_.current_joint_count(ahamkara::client::weapon_viewmodel_mesh_path(weapon_index));
}

void WeaponViewmodelPresentation::tick(float dt) {
    cache_.tick(dt);
}

// ============================================================
// Viewmodel arm IK solver
// ============================================================
//
// Analytical two-bone IK for the viewmodel arm skeleton:
//   shoulder (joint 2) → elbow (joint 3) → hand (joint 5)
//
// Joint indices match the viewmodel_arms.gltf skin layout:
//   [0]=Bone, [1]=root, [2]=shoulder, [3]=elbow,
//   [4]=wrist, [5]=hand, [6]=weapon_attach
//
// The solver operates in model space (before the weapon animation transform
// is applied as the model matrix) and works with the arm's +Y convention
// (the arm extends along +Y from shoulder to hand in the bind pose).
// ============================================================

namespace {

constexpr int kShoulderJoint = 2;
constexpr int kElbowJoint   = 3;
constexpr int kHandJoint    = 5;

// Bone lengths from viewmodel_arms.gltf bind pose (in meters).
// shoulder → elbow: translation [0, 0.35, 0]
// elbow → hand:    translation [0, 0.28, 0] + [0, 0.06, 0] = [0, 0.34, 0]
constexpr float kUpperArmLen = 0.35F;
constexpr float kLowerArmLen = 0.34F;

/// Solve two-bone IK in the viewmodel arm's coordinate space (+Y extends
/// downward from shoulder).  Produces quaternion corrections for the
/// shoulder (root) and elbow (mid) joints.
void solve_two_bone_arm(
    float target_x, float target_y, float target_z,
    float upper_len, float lower_len,
    float& out_root_qx, float& out_root_qy,
    float& out_root_qz, float& out_root_qw,
    float& out_mid_qx, float& out_mid_qy,
    float& out_mid_qz, float& out_mid_qw) {

    // Distance from shoulder to target
    float dist = std::sqrt(target_x * target_x +
                            target_y * target_y +
                            target_z * target_z);
    if (dist < 0.0001F) {
        // Target coincides with shoulder — no correction needed
        out_root_qx = 0.0F; out_root_qy = 0.0F;
        out_root_qz = 0.0F; out_root_qw = 1.0F;
        out_mid_qx = 0.0F; out_mid_qy = 0.0F;
        out_mid_qz = 0.0F; out_mid_qw = 1.0F;
        return;
    }

    // Clamp to reachable range
    float max_reach = upper_len + lower_len;
    float min_reach = std::abs(upper_len - lower_len);
    dist = std::clamp(dist, min_reach, max_reach);

    // Law of cosines: elbow angle
    // cos(C) = (L1² + L2² - dist²) / (2 * L1 * L2)
    float cos_elbow = (upper_len * upper_len + lower_len * lower_len - dist * dist)
                      / (2.0F * upper_len * lower_len);
    cos_elbow = std::clamp(cos_elbow, -1.0F, 1.0F);
    float elbow_angle = std::acos(cos_elbow);
    float mid_bend = 3.14159265F - elbow_angle;

    // Direction from shoulder to target (normalized)
    float dir_x = target_x / dist;
    float dir_y = target_y / dist;
    float dir_z = target_z / dist;

    // Default arm direction in bind pose: +Y (extends downward)
    // This matches the viewmodel_arms skeleton hierarchy.
    constexpr float kDefaultDirX = 0.0F;
    constexpr float kDefaultDirY = 1.0F;
    constexpr float kDefaultDirZ = 0.0F;

    // Angle between default direction and target direction
    float cos_aim = kDefaultDirX * dir_x + kDefaultDirY * dir_y + kDefaultDirZ * dir_z;
    cos_aim = std::clamp(cos_aim, -1.0F, 1.0F);
    float aim_angle = std::acos(cos_aim);

    // Shoulder over-rotation angle: the extra rotation beyond aiming at the
    // target, needed so the elbow bends the correct amount.
    // cos(A) = (L1² + dist² - L2²) / (2 * L1 * dist)
    float cos_root = (upper_len * upper_len + dist * dist - lower_len * lower_len)
                     / (2.0F * upper_len * dist);
    cos_root = std::clamp(cos_root, -1.0F, 1.0F);
    float root_angle = std::acos(cos_root);

    // Rotation axis: cross(default_dir, target_dir)
    float axis_x = kDefaultDirY * dir_z - kDefaultDirZ * dir_y;
    float axis_y = kDefaultDirZ * dir_x - kDefaultDirX * dir_z;
    float axis_z = kDefaultDirX * dir_y - kDefaultDirY * dir_x;
    float axis_len = std::sqrt(axis_x * axis_x + axis_y * axis_y + axis_z * axis_z);

    if (axis_len > 0.0001F) {
        axis_x /= axis_len;
        axis_y /= axis_len;
        axis_z /= axis_len;
    } else {
        // Default and target are parallel or anti-parallel
        axis_x = 0.0F; axis_y = 0.0F; axis_z = 1.0F;
    }

    // Root rotation: rotate by (aim_angle - root_angle) around the axis.
    // This aims the upper arm so the elbow bends to reach the target.
    float total_root = aim_angle - root_angle;
    float half_root = total_root * 0.5F;
    float sin_half = std::sin(half_root);
    out_root_qx = axis_x * sin_half;
    out_root_qy = axis_y * sin_half;
    out_root_qz = axis_z * sin_half;
    out_root_qw = std::cos(half_root);

    // Mid joint (elbow) bends around the local Z axis
    float half_mid = mid_bend * 0.5F;
    out_mid_qx = 0.0F;
    out_mid_qy = 0.0F;
    out_mid_qz = std::sin(half_mid);
    out_mid_qw = std::cos(half_mid);
}

}  // namespace

void apply_viewmodel_arm_ik(int weapon_index,
                             float* joint_matrices,
                             int joint_count) {
    // Need at least up to hand joint (index 5 = 6 joints * 16 floats)
    if (joint_count <= kHandJoint || joint_matrices == nullptr) {
        return;
    }

    // Extract shoulder position from joint matrix (column-major, translation
    // is at m[12], m[13], m[14] for col 3 rows 0-2).
    float shoulder_x = joint_matrices[kShoulderJoint * 16 + 12];
    float shoulder_y = joint_matrices[kShoulderJoint * 16 + 13];
    float shoulder_z = joint_matrices[kShoulderJoint * 16 + 14];

    // Get the grip socket position for this weapon (viewmodel-local space).
    auto grip = weapon_grip_sockets(weapon_index);

    // Compute target position relative to the shoulder joint.
    float tx = grip.grip_right_x - shoulder_x;
    float ty = grip.grip_right_y - shoulder_y;
    float tz = grip.grip_right_z - shoulder_z;

    // Solve the two-bone IK chain.
    float rqx, rqy, rqz, rqw;
    float mqx, mqy, mqz, mqw;
    solve_two_bone_arm(tx, ty, tz,
                       kUpperArmLen, kLowerArmLen,
                       rqx, rqy, rqz, rqw,
                       mqx, mqy, mqz, mqw);

    // Build rotation matrices from quaternion corrections.
    ae::render::Mat4 root_rot = ae::render::Mat4::rotation_quat(rqx, rqy, rqz, rqw);
    ae::render::Mat4 mid_rot  = ae::render::Mat4::rotation_quat(mqx, mqy, mqz, mqw);

    // Apply shoulder correction: new_joint = correction * old_joint.
    {
        float* mat = joint_matrices + kShoulderJoint * 16;
        ae::render::Mat4 old;
        std::memcpy(old.m.data(), mat, 16 * sizeof(float));
        ae::render::Mat4 updated = root_rot * old;
        std::memcpy(mat, updated.m.data(), 16 * sizeof(float));
    }

    // Apply elbow correction.
    {
        float* mat = joint_matrices + kElbowJoint * 16;
        ae::render::Mat4 old;
        std::memcpy(old.m.data(), mat, 16 * sizeof(float));
        ae::render::Mat4 updated = mid_rot * old;
        std::memcpy(mat, updated.m.data(), 16 * sizeof(float));
    }
}

}  // namespace ahamkara::client

