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
        0.0F, 0.0F, 0.0F, 1.0F,
        0.0F, s, 0.0F, s,
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

bool nearly_equal(float a, float b, float eps = 1.0e-3F) {
    return std::fabs(a - b) <= eps;
}

} // namespace

int main() {
    ae::skeleton::ProceduralAnimState proc{};
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

    std::vector<ae::skeleton::Mat4> pose_vec{pose[0]};
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

    std::cout << "animation_headless_tests: ok\n";
    return 0;
}
