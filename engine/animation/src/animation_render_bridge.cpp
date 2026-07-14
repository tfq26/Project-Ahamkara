#include "ae/animation/animation_render_bridge.h"
#include "ae/animation/animation_driver.h"
#include "ae/skeleton/types.h"
#include <cstring>
#include <vector>

namespace ae::animation {

AnimationRenderOutput extract_joint_matrices(const std::vector<ae::skeleton::Mat4>& pose) {
    AnimationRenderOutput out;
    out.joint_count = static_cast<int>(pose.size());
    if (out.joint_count == 0) return out;

    out.joint_matrices.resize(out.joint_count * 16);
    for (int i = 0; i < out.joint_count; ++i) {
        std::memcpy(&out.joint_matrices[i * 16], &pose[i].m[0], 16 * sizeof(float));
    }
    return out;
}

} // namespace ae::animation
