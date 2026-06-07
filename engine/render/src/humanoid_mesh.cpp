#include "ae/render/humanoid_mesh.h"

#include <cmath>

namespace ae::render {
namespace {

void add_box_colored(std::vector<float>& pos, std::vector<float>& nrm, 
                     std::vector<float>& j_idx, std::vector<float>& j_wt,
                     std::vector<std::uint32_t>& idx,
                     float cx, float cy, float cz, float hw, float hd, 
                     float top_y, float bot_y, int joint_index) {
    const float x0 = cx - hw, x1 = cx + hw;
    const float z0 = cz - hd, z1 = cz + hd;
    const float y0 = bot_y,  y1 = top_y;
    const std::uint32_t base = static_cast<std::uint32_t>(pos.size() / 3);

    float verts[8][3] = {
        {x0, y0, z1}, {x1, y0, z1}, {x1, y0, z0}, {x0, y0, z0},
        {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0},
    };
    for (int i = 0; i < 8; ++i) {
        pos.push_back(verts[i][0]); pos.push_back(verts[i][1]); pos.push_back(verts[i][2]);
        nrm.push_back(0); nrm.push_back(1); nrm.push_back(0);
        // Joint index (4 values)
        j_idx.push_back(static_cast<float>(joint_index));
        j_idx.push_back(0.0F);
        j_idx.push_back(0.0F);
        j_idx.push_back(0.0F);
        // Joint weight (4 values)
        j_wt.push_back(1.0F);
        j_wt.push_back(0.0F);
        j_wt.push_back(0.0F);
        j_wt.push_back(0.0F);
    }

    auto push_quad = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d,
                          float nx, float ny, float nz) {
        for (auto v : {a, b, c, d}) {
            nrm[static_cast<std::size_t>(v) * 3 + 0] = nx;
            nrm[static_cast<std::size_t>(v) * 3 + 1] = ny;
            nrm[static_cast<std::size_t>(v) * 3 + 2] = nz;
        }
        idx.push_back(base + a); idx.push_back(base + b); idx.push_back(base + c);
        idx.push_back(base + a); idx.push_back(base + c); idx.push_back(base + d);
    };

    push_quad(0, 1, 5, 4,  0, 0, 1);   // Front
    push_quad(2, 3, 7, 6,  0, 0, -1);  // Back
    push_quad(1, 2, 6, 5,  1, 0, 0);   // Right
    push_quad(3, 0, 4, 7, -1, 0, 0);   // Left
    push_quad(4, 5, 6, 7,  0, 1, 0);   // Top
    push_quad(3, 2, 1, 0,  0, -1, 0);  // Bottom
}

GltfMesh make_part(float r, float g, float b) {
    GltfMesh m;
    m.color_r = r; m.color_g = g; m.color_b = b;
    m.has_material_color = true;
    return m;
}

}  // namespace

GltfModel generate_humanoid_mesh(HumanoidLod lod) {
    GltfModel model;

    // Proportions (total height = 1.0)
    const float foot_y   = 0.00F;
    const float knee_y   = 0.23F;
    const float hip_y    = 0.47F;
    const float chest_y  = 0.62F;
    const float shoulder_y = 0.75F;
    const float neck_y   = 0.80F;
    const float head_top = 0.95F;

    if (lod == HumanoidLod::Low) {
        // --- LOD2: Minimal — single box capsule (torso+head+legs combined) ---
        // joint 0: Root (no animation, just a static placeholder)
        {
            auto m = make_part(0.22F, 0.45F, 0.70F);
            add_box_colored(m.positions, m.normals, m.joint_indices, m.joint_weights, m.indices,
                            0.0F, 0, 0.0F, 0.12F, 0.08F, head_top, foot_y, 0);
            model.meshes.push_back(std::move(m));
        }
        return model;
    }

    if (lod == HumanoidLod::Medium) {
        // --- LOD1: Medium — 4 parts (head+torso, hips+legs, 2 arms) ---

        // Head + neck + torso combined (joint 2: Spine)
        {
            auto m = make_part(1.0F, 0.88F, 0.65F);
            add_box_colored(m.positions, m.normals, m.joint_indices, m.joint_weights, m.indices,
                            0.0F, 0, 0.0F, 0.055F, 0.055F, head_top, neck_y, 2);
            model.meshes.push_back(std::move(m));
        }
        {
            auto m = make_part(0.22F, 0.45F, 0.70F);
            add_box_colored(m.positions, m.normals, m.joint_indices, m.joint_weights, m.indices,
                            0.0F, 0, 0.0F, 0.10F, 0.06F, shoulder_y, chest_y, 2);
            add_box_colored(m.positions, m.normals, m.joint_indices, m.joint_weights, m.indices,
                            0.0F, 0, 0.0F, 0.08F, 0.05F, chest_y, hip_y, 2);
            model.meshes.push_back(std::move(m));
        }

        // Hips + both legs combined (joint 1: Hips) — no feet
        {
            auto m = make_part(0.15F, 0.22F, 0.28F);
            add_box_colored(m.positions, m.normals, m.joint_indices, m.joint_weights, m.indices,
                            0.0F, 0, 0.0F, 0.075F, 0.05F, hip_y, foot_y, 1);
            model.meshes.push_back(std::move(m));
        }

        // Left arm (joint 4)
        {
            auto m = make_part(0.22F, 0.45F, 0.70F);
            add_box_colored(m.positions, m.normals, m.joint_indices, m.joint_weights, m.indices,
                            -0.13F, 0, 0.0F, 0.025F, 0.025F, shoulder_y, shoulder_y - 0.30F, 4);
            model.meshes.push_back(std::move(m));
        }

        // Right arm (joint 5)
        {
            auto m = make_part(0.22F, 0.45F, 0.70F);
            add_box_colored(m.positions, m.normals, m.joint_indices, m.joint_weights, m.indices,
                            0.13F, 0, 0.0F, 0.025F, 0.025F, shoulder_y, shoulder_y - 0.30F, 5);
            model.meshes.push_back(std::move(m));
        }
        return model;
    }

    // --- LOD0: High detail (original) ---

    // --- Head (skin tone) ---
    // joint 3: Head
    {
        auto m = make_part(1.0F, 0.88F, 0.65F);
        add_box_colored(m.positions, m.normals, m.joint_indices, m.joint_weights, m.indices,
                        0.0F, 0, 0.0F, 0.055F, 0.055F, head_top, neck_y, 3);
        model.meshes.push_back(std::move(m));
    }

    // --- Torso (orange shirt) ---
    // joint 2: Spine
    {
        auto m = make_part(0.22F, 0.45F, 0.70F);
        add_box_colored(m.positions, m.normals, m.joint_indices, m.joint_weights, m.indices,
                        0.0F, 0, 0.0F, 0.10F, 0.06F, shoulder_y, chest_y, 2);
        add_box_colored(m.positions, m.normals, m.joint_indices, m.joint_weights, m.indices,
                        0.0F, 0, 0.0F, 0.08F, 0.05F, chest_y, hip_y, 2);
        model.meshes.push_back(std::move(m));
    }

    // --- Hips / belt (dark) ---
    // joint 1: Hips
    {
        auto m = make_part(0.15F, 0.18F, 0.22F);
        add_box_colored(m.positions, m.normals, m.joint_indices, m.joint_weights, m.indices,
                        0.0F, 0, 0.0F, 0.085F, 0.055F, hip_y, hip_y - 0.06F, 1);
        model.meshes.push_back(std::move(m));
    }

    // --- Left arm (shirt sleeve color) ---
    // joint 4: Left arm
    {
        auto m = make_part(0.22F, 0.45F, 0.70F);
        add_box_colored(m.positions, m.normals, m.joint_indices, m.joint_weights, m.indices,
                        -0.13F, 0, 0.0F, 0.025F, 0.025F, shoulder_y, shoulder_y - 0.30F, 4);
        model.meshes.push_back(std::move(m));
    }

    // --- Right arm ---
    // joint 5: Right arm
    {
        auto m = make_part(0.22F, 0.45F, 0.70F);
        add_box_colored(m.positions, m.normals, m.joint_indices, m.joint_weights, m.indices,
                        0.13F, 0, 0.0F, 0.025F, 0.025F, shoulder_y, shoulder_y - 0.30F, 5);
        model.meshes.push_back(std::move(m));
    }

    // --- Left leg (dark pants) ---
    // joint 6: Left leg
    {
        auto m = make_part(0.18F, 0.22F, 0.28F);
        add_box_colored(m.positions, m.normals, m.joint_indices, m.joint_weights, m.indices,
                        -0.045F, 0, 0.0F, 0.035F, 0.035F, hip_y - 0.06F, knee_y, 6);
        add_box_colored(m.positions, m.normals, m.joint_indices, m.joint_weights, m.indices,
                        -0.045F, 0, 0.0F, 0.030F, 0.030F, knee_y, foot_y, 6);
        model.meshes.push_back(std::move(m));
    }

    // --- Right leg ---
    // joint 7: Right leg
    {
        auto m = make_part(0.18F, 0.22F, 0.28F);
        add_box_colored(m.positions, m.normals, m.joint_indices, m.joint_weights, m.indices,
                        0.045F, 0, 0.0F, 0.035F, 0.035F, hip_y - 0.06F, knee_y, 7);
        add_box_colored(m.positions, m.normals, m.joint_indices, m.joint_weights, m.indices,
                        0.045F, 0, 0.0F, 0.030F, 0.030F, knee_y, foot_y, 7);
        model.meshes.push_back(std::move(m));
    }

    // --- Feet (dark shoes) ---
    // left foot -> joint 6, right foot -> joint 7
    {
        auto m = make_part(0.12F, 0.12F, 0.14F);
        add_box_colored(m.positions, m.normals, m.joint_indices, m.joint_weights, m.indices,
                        -0.045F, 0, 0.03F, 0.035F, 0.045F, foot_y + 0.04F, foot_y, 6);
        add_box_colored(m.positions, m.normals, m.joint_indices, m.joint_weights, m.indices,
                        0.045F, 0, 0.03F, 0.035F, 0.045F, foot_y + 0.04F, foot_y, 7);
        model.meshes.push_back(std::move(m));
    }

    return model;
}

}  // namespace ae::render
