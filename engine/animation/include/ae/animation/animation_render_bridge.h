#pragma once

#include "ae/skeleton/types.h"
#include <vector>

namespace ae::animation {
struct AnimationDriver;
struct AnimationRenderOutput {
    std::vector<float> joint_matrices;
    int joint_count = 0;
};
/// Extract joint matrices from a pose vector into a flat float array for GPU skinning.
/// The pose vector is produced by AnimationDriver::tick() or CharacterAnimInstance::tick().
AnimationRenderOutput extract_joint_matrices(const std::vector<ae::skeleton::Mat4>& pose);
} // namespace ae::animation
