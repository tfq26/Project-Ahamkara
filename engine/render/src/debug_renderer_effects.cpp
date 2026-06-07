// SPDX-License-Identifier: MIT
// Particle and decal rendering for the debug renderer.

#include "debug_renderer_internal.h"

#include <cmath>
#include <vector>

namespace ae::render {

void draw_particles(RenderBackend& backend, BufferHandle& particle_vbo,
                    BufferHandle& particle_color_vbo, RenderStats& stats,
                    const DebugScene& scene, const LocalMat4& view,
                    const Frustum& frustum) {
    if (scene.particle_count == 0) return;

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive blending for particles
    glDepthMask(GL_FALSE); // particles don't write to depth

    std::vector<float> positions;
    std::vector<float> colors;
    positions.reserve(static_cast<std::size_t>(scene.particle_count) * 18);
    colors.reserve(static_cast<std::size_t>(scene.particle_count) * 24);

    int active_count = 0;
    for (int i = 0; i < scene.particle_count && i < 256; ++i) {
        float alpha = scene.particle_alphas[i];
        if (alpha <= 0.0F) continue;

        Vec3 pos = scene.particle_positions[i];
        float size = scene.particle_sizes[i];

        // Frustum culling: skip particles outside the view frustum
        if (!frustum.intersects_sphere(pos.x, pos.y, pos.z, size)) {
            continue;
        }

        float rx = view.values[0], ry = view.values[4], rz = view.values[8];
        float ux = view.values[1], uy = view.values[5], uz = view.values[9];

        float rlen = std::sqrt(rx*rx + ry*ry + rz*rz);
        float ulen = std::sqrt(ux*ux + uy*uy + uz*uz);
        if (rlen > 0.0F) { rx /= rlen; ry /= rlen; rz /= rlen; }
        if (ulen > 0.0F) { ux /= ulen; uy /= ulen; uz /= ulen; }

        float s = size * 0.5F;
        float cr = scene.particle_colors_r[i];
        float cg = scene.particle_colors_g[i];
        float cb = scene.particle_colors_b[i];

        Vec3 v0 = {pos.x - rx*s - ux*s, pos.y - ry*s - uy*s, pos.z - rz*s - uz*s};
        Vec3 v1 = {pos.x + rx*s - ux*s, pos.y + ry*s - uy*s, pos.z + rz*s - uz*s};
        Vec3 v2 = {pos.x + rx*s + ux*s, pos.y + ry*s + uy*s, pos.z + rz*s + uz*s};
        Vec3 v3 = {pos.x - rx*s + ux*s, pos.y - ry*s + uy*s, pos.z - rz*s + uz*s};

        // Triangle 1
        positions.push_back(v0.x); positions.push_back(v0.y); positions.push_back(v0.z);
        positions.push_back(v1.x); positions.push_back(v1.y); positions.push_back(v1.z);
        positions.push_back(v2.x); positions.push_back(v2.y); positions.push_back(v2.z);

        // Triangle 2
        positions.push_back(v0.x); positions.push_back(v0.y); positions.push_back(v0.z);
        positions.push_back(v2.x); positions.push_back(v2.y); positions.push_back(v2.z);
        positions.push_back(v3.x); positions.push_back(v3.y); positions.push_back(v3.z);

        for (int k = 0; k < 6; ++k) {
            colors.push_back(cr);
            colors.push_back(cg);
            colors.push_back(cb);
            colors.push_back(alpha);
        }
        active_count++;
    }

    if (active_count > 0) {
        // Lazy-create or update dynamic vertex buffers via backend
        if (!particle_vbo) {
            particle_vbo = backend.create_vertex_buffer(nullptr, positions.size() * sizeof(float), true);
        }
        if (!particle_color_vbo) {
            particle_color_vbo = backend.create_vertex_buffer(nullptr, colors.size() * sizeof(float), true);
        }

        backend.update_vertex_buffer(particle_vbo, positions.data(), positions.size() * sizeof(float));
        backend.update_vertex_buffer(particle_color_vbo, colors.data(), colors.size() * sizeof(float));

        backend.draw_arrays_vnc(particle_vbo, kInvalidBuffer, particle_color_vbo, 0, active_count * 6, 4);

        stats.drawn_particle_count = active_count;
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

void draw_decals(RenderBackend& backend, BufferHandle& decal_vbo,
                 BufferHandle& decal_color_vbo, RenderStats& stats,
                 const DebugScene& scene, const Frustum& frustum) {
    if (scene.decal_count == 0) return;

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0F, -1.0F);

    std::vector<float> positions;
    std::vector<float> colors;
    positions.reserve(static_cast<std::size_t>(scene.decal_count) * 18);
    colors.reserve(static_cast<std::size_t>(scene.decal_count) * 24);

    int active_count = 0;
    for (int i = 0; i < scene.decal_count && i < 64; ++i) {
        Vec3 pos = scene.decal_positions[i];
        float size = scene.decal_sizes[i] * 0.5F;

        // Frustum culling for decals
        if (!frustum.intersects_sphere(pos.x, pos.y, pos.z, size * 2.0F)) {
            continue;
        }

        Vec3 n = scene.decal_normals[i];

        Vec3 tangent, bitangent;
        if (std::fabs(n.x) < 0.9F) { tangent = {n.z, 0.0F, -n.x}; }
        else { tangent = {0.0F, -n.z, n.y}; }
        float tlen = std::sqrt(tangent.x*tangent.x + tangent.y*tangent.y + tangent.z*tangent.z);
        if (tlen > 0.0F) { tangent.x /= tlen; tangent.y /= tlen; tangent.z /= tlen; }
        bitangent.x = n.y*tangent.z - n.z*tangent.y;
        bitangent.y = n.z*tangent.x - n.x*tangent.z;
        bitangent.z = n.x*tangent.y - n.y*tangent.x;

        Vec3 v0 = {pos.x - tangent.x*size - bitangent.x*size, pos.y - tangent.y*size - bitangent.y*size, pos.z - tangent.z*size - bitangent.z*size};
        Vec3 v1 = {pos.x + tangent.x*size - bitangent.x*size, pos.y + tangent.y*size - bitangent.y*size, pos.z + tangent.z*size - bitangent.z*size};
        Vec3 v2 = {pos.x + tangent.x*size + bitangent.x*size, pos.y + tangent.y*size + bitangent.y*size, pos.z + tangent.z*size + bitangent.z*size};
        Vec3 v3 = {pos.x - tangent.x*size + bitangent.x*size, pos.y - tangent.y*size + bitangent.y*size, pos.z - tangent.z*size + bitangent.z*size};

        // Triangle 1
        positions.push_back(v0.x); positions.push_back(v0.y); positions.push_back(v0.z);
        positions.push_back(v1.x); positions.push_back(v1.y); positions.push_back(v1.z);
        positions.push_back(v2.x); positions.push_back(v2.y); positions.push_back(v2.z);

        // Triangle 2
        positions.push_back(v0.x); positions.push_back(v0.y); positions.push_back(v0.z);
        positions.push_back(v2.x); positions.push_back(v2.y); positions.push_back(v2.z);
        positions.push_back(v3.x); positions.push_back(v3.y); positions.push_back(v3.z);

        for (int k = 0; k < 6; ++k) {
            colors.push_back(0.05F);
            colors.push_back(0.05F);
            colors.push_back(0.07F);
            colors.push_back(0.85F);
        }
        active_count++;
    }

    if (active_count > 0) {
        // Lazy-create or update dynamic vertex buffers via backend
        if (!decal_vbo) {
            decal_vbo = backend.create_vertex_buffer(nullptr, positions.size() * sizeof(float), true);
        }
        if (!decal_color_vbo) {
            decal_color_vbo = backend.create_vertex_buffer(nullptr, colors.size() * sizeof(float), true);
        }

        backend.update_vertex_buffer(decal_vbo, positions.data(), positions.size() * sizeof(float));
        backend.update_vertex_buffer(decal_color_vbo, colors.data(), colors.size() * sizeof(float));

        backend.draw_arrays_vnc(decal_vbo, kInvalidBuffer, decal_color_vbo, 0, active_count * 6, 4);

        stats.drawn_decal_count = active_count;
    }

    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

}  // namespace ae::render
