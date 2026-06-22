#pragma once

#include <vector>

namespace ae::animation {
struct AnimationDriver;
struct AnimationRenderOutput {
    std::vector<float> joint_matrices;
    int joint_count = 0;
};
AnimationRenderOutput extract_joint_matrices(const AnimationDriver& driver);
} // namespace ae::animation
