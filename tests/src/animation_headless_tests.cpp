#include "ae/animation/clip_player.h"
#include "ae/skeleton/types.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace {

ae::skeleton::AnimationClipData make_rotation_clip() {
    ae::skeleton::AnimationClipData clip;
    clip.name = "spin";
    ae::skeleton::AnimationSampler sampler;
    sampler.input_times = {0.0F, 1.0F};
    const float s = 0.70710678F;
    sampler.output_values = {
        0.0F,
        0.0F,
        0.0F,
        1.0F,
        0.0F,
        s,
        0.0F,
        s,
    };
    sampler.interpolation = "LINEAR";
    clip.samplers.push_back(sampler);

    ae::skeleton::AnimationChannel channel;
    channel.node_index = 0;
    channel.path = "rotation";
    channel.sampler_index = 0;
    clip.channels.push_back(channel);
    return clip;
}

ae::skeleton::Skin make_single_joint_skin() {
    ae::skeleton::Skin skin;
    ae::skeleton::Joint joint;
    joint.name = "root";
    joint.parent_index = -1;
    joint.inverse_bind_matrix = ae::skeleton::Mat4::identity();
    skin.joints.push_back(joint);
    return skin;
}

ae::skeleton::Skin make_child_first_skin() {
    ae::skeleton::Skin skin;
    ae::skeleton::Joint child;
    child.name = "child";
    child.node_index = 20;
    child.parent_index = 1;
    child.inverse_bind_matrix = ae::skeleton::Mat4::identity();
    skin.joints.push_back(child);

    ae::skeleton::Joint root;
    root.name = "root";
    root.node_index = 10;
    root.parent_index = -1;
    root.inverse_bind_matrix = ae::skeleton::Mat4::identity();
    skin.joints.push_back(root);
    return skin;
}

ae::skeleton::Skin make_bind_pose_skin() {
    ae::skeleton::Skin skin;
    ae::skeleton::Joint joint;
    joint.name = "bound_root";
    joint.node_index = 30;
    joint.bind_translation = {2.0F, 3.0F, 4.0F};
    joint.inverse_bind_matrix = ae::skeleton::Mat4::identity();
    skin.joints.push_back(joint);
    return skin;
}

ae::skeleton::AnimationClipData make_rotation_only_clip() {
    ae::skeleton::AnimationClipData clip;
    clip.name = "rotation_only";
    ae::skeleton::AnimationSampler sampler;
    sampler.input_times = {0.0F};
    sampler.output_values = {0.0F, 0.0F, 0.0F, 1.0F};
    sampler.interpolation = "LINEAR";
    clip.samplers.push_back(sampler);
    clip.channels.push_back({30, "rotation", 0});
    return clip;
}

ae::skeleton::AnimationClipData make_child_first_translation_clip() {
    ae::skeleton::AnimationClipData clip;
    clip.name = "child_first";
    for (const auto [node, translation] : {std::pair {10, 1.0F}, std::pair {20, 2.0F}}) {
        ae::skeleton::AnimationSampler sampler;
        sampler.input_times = {0.0F};
        sampler.output_values = {translation, 0.0F, 0.0F};
        sampler.interpolation = "LINEAR";
        clip.samplers.push_back(sampler);

        ae::skeleton::AnimationChannel channel;
        channel.node_index = node;
        channel.path = "translation";
        channel.sampler_index = static_cast<int>(clip.samplers.size() - 1U);
        clip.channels.push_back(channel);
    }
    return clip;
}

bool nearly_equal(float a, float b, float eps = 1.0e-3F) {
    return std::fabs(a - b) <= eps;
}

} // namespace

int main() {
    ae::skeleton::ProceduralAnimState proc {};
    std::vector<ae::skeleton::Mat4> procedural;
    ae::skeleton::evaluate_procedural_animation(proc, 1.0F / 60.0F, procedural);
    if (procedural.size() != 8U) {
        std::cerr << "expected 8 procedural joints, got " << procedural.size() << "\n";
        return 1;
    }

    ae::animation::AnimationClipPlayer player;
    if (!player.set_data(make_single_joint_skin(), {make_rotation_clip()})) {
        std::cerr << "failed to set clip player data\n";
        return 1;
    }
    player.play("spin", true);
    const ae::skeleton::Mat4* pose = player.tick(0.5F);
    if (pose == nullptr) {
        std::cerr << "tick returned null pose\n";
        return 1;
    }
    if (player.joint_count() != 1) {
        std::cerr << "unexpected joint count\n";
        return 1;
    }

    std::vector<ae::skeleton::Mat4> pose_vec {pose[0]};
    const auto palette = ae::skeleton::extract_pose_palette(pose_vec);
    if (palette.joint_count != 1 || palette.joint_matrices.size() != 16U) {
        std::cerr << "palette contract failed\n";
        return 1;
    }
    if (!nearly_equal(palette.joint_matrices[12], 0.0F) ||
        !nearly_equal(palette.joint_matrices[13], 0.0F) ||
        !nearly_equal(palette.joint_matrices[14], 0.0F)) {
        std::cerr << "unexpected translation in palette\n";
        return 1;
    }

    // Regression: skin arrays may list a child before its parent. The child
    // must still inherit the parent's transform (1 + 2 = 3 units on X).
    std::vector<ae::skeleton::Mat4> child_first_pose;
    ae::skeleton::evaluate_animation(
        make_child_first_translation_clip(),
        make_child_first_skin(),
        0.0F,
        child_first_pose);
    if (child_first_pose.size() != 2U ||
        !nearly_equal(child_first_pose[0].m[12], 3.0F) ||
        !nearly_equal(child_first_pose[1].m[12], 1.0F)) {
        std::cerr << "child-first hierarchy evaluation failed\n";
        return 1;
    }

    // A rotation-only clip must retain an imported node's bind translation.
    // Blender/MPFB skeletons use non-identity rest transforms for every limb.
    std::vector<ae::skeleton::Mat4> bind_pose;
    ae::skeleton::evaluate_animation(
        make_rotation_only_clip(), make_bind_pose_skin(), 0.0F, bind_pose);
    if (bind_pose.size() != 1U ||
        !nearly_equal(bind_pose[0].m[12], 2.0F) ||
        !nearly_equal(bind_pose[0].m[13], 3.0F) ||
        !nearly_equal(bind_pose[0].m[14], 4.0F)) {
        std::cerr << "bind-pose transform evaluation failed\n";
        return 1;
    }

    std::cout << "animation_headless_tests: ok\n";
    return 0;
}
