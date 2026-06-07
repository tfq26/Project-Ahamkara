// SPDX-License-Identifier: MIT
// GPU model drawing helper for the debug renderer.

#include "debug_renderer_internal.h"

namespace ae::render {

void draw_gpu_model(RenderBackend& backend,
                    const GpuModel& model,
                    ShaderHandle shader,
                    int color_loc,
                    int use_skinning_loc,
                    int joints_mat_loc,
                    float r, float g, float b, float a,
                    bool use_mesh_colors,
                    const ae::render::Mat4* joint_matrices,
                    int joint_count) {
    bool has_skinning = (joint_matrices != nullptr && joint_count > 0);

    if (shader) {
        backend.use_shader(shader);
        backend.set_uniform_int(use_skinning_loc, has_skinning ? 1 : 0);
        if (has_skinning) {
            backend.set_uniform_mat4_array(joints_mat_loc,
                                           reinterpret_cast<const float*>(joint_matrices),
                                           joint_count);
        }
    }

    for (const auto& mesh : model.meshes) {
        if (mesh.vertex_count == 0) continue;

        float final_r = use_mesh_colors ? mesh.color_r : r;
        float final_g = use_mesh_colors ? mesh.color_g : g;
        float final_b = use_mesh_colors ? mesh.color_b : b;

        if (shader && color_loc != -1) {
            backend.set_uniform_vec4(color_loc, final_r, final_g, final_b, a);
        } else {
            glColor4f(final_r, final_g, final_b, a);
        }

        if (has_skinning) {
            backend.draw_gpu_mesh_skinned(mesh);
        } else {
            backend.draw_gpu_mesh(mesh);
        }
    }

    if (shader) {
        backend.use_shader(kInvalidShader);
    }
}

}  // namespace ae::render
