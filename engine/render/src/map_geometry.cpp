#include "ae/render/map_geometry.h"
#include "ae/core/log.h"

#include <algorithm>
#include <cmath>
#include "ae/render/gl_platform.h"

#define AE_LOG_CATEGORY "Render"

namespace ae::render {
namespace {

constexpr float kCellSize = 8.0F;  // 32-unit arena / 4 cells

// ============================================================
// Per-cell geometry builders
// ============================================================
struct CellBuilder {
    std::vector<float> tri_positions;
    std::vector<float> tri_normals;
    std::vector<float> tri_colors;
    std::vector<float> line_positions;
    std::vector<float> line_colors;
    float min_x {1e9F}, min_y {1e9F}, min_z {1e9F};
    float max_x {-1e9F}, max_y {-1e9F}, max_z {-1e9F};

    // Cell spatial bounds (set by build(), immutable after)
    float cell_min_x {0}, cell_max_x {0}, cell_min_z {0}, cell_max_z {0};

    bool overlaps_xz(float bx_min_x, float bx_max_x, float bx_min_z, float bx_max_z) const {
        return !(bx_max_x < cell_min_x || bx_min_x > cell_max_x ||
                 bx_max_z < cell_min_z || bx_min_z > cell_max_z);
    }

    void add_box(float minx, float miny, float minz,
                 float maxx, float maxy, float maxz,
                 float r, float g, float b) {
        if (!overlaps_xz(minx, maxx, minz, maxz)) return;

        // Expand bounds
        min_x = std::min(min_x, minx); max_x = std::max(max_x, maxx);
        min_y = std::min(min_y, miny); max_y = std::max(max_y, maxy);
        min_z = std::min(min_z, minz); max_z = std::max(max_z, maxz);

        // Each box has 6 faces, each face = 2 triangles = 6 vertices
        // 6 faces × 6 vertices = 36 vertices total
        auto push_face = [&](float nx, float ny, float nz,
                             float x0, float y0, float z0,
                             float x1, float y1, float z1,
                             float x2, float y2, float z2,
                             float x3, float y3, float z3) {
            // Triangle 1: v0, v1, v2
            for (int v = 0; v < 6; ++v) {
                tri_colors.push_back(r);
                tri_colors.push_back(g);
                tri_colors.push_back(b);
                tri_normals.push_back(nx);
                tri_normals.push_back(ny);
                tri_normals.push_back(nz);
            }
            tri_positions.push_back(x0); tri_positions.push_back(y0); tri_positions.push_back(z0);
            tri_positions.push_back(x1); tri_positions.push_back(y1); tri_positions.push_back(z1);
            tri_positions.push_back(x2); tri_positions.push_back(y2); tri_positions.push_back(z2);
            // Triangle 2: v0, v2, v3
            tri_positions.push_back(x0); tri_positions.push_back(y0); tri_positions.push_back(z0);
            tri_positions.push_back(x2); tri_positions.push_back(y2); tri_positions.push_back(z2);
            tri_positions.push_back(x3); tri_positions.push_back(y3); tri_positions.push_back(z3);
        };

        // +Z (front)
        push_face(0, 0, 1, minx, miny, maxz, maxx, miny, maxz, maxx, maxy, maxz, minx, maxy, maxz);
        // -Z (back)
        push_face(0, 0, -1, maxx, miny, minz, minx, miny, minz, minx, maxy, minz, maxx, maxy, minz);
        // +X (right)
        push_face(1, 0, 0, maxx, miny, maxz, maxx, miny, minz, maxx, maxy, minz, maxx, maxy, maxz);
        // -X (left)
        push_face(-1, 0, 0, minx, miny, minz, minx, miny, maxz, minx, maxy, maxz, minx, maxy, minz);
        // +Y (top)
        push_face(0, 1, 0, minx, maxy, maxz, maxx, maxy, maxz, maxx, maxy, minz, minx, maxy, minz);
        // -Y (bottom)
        push_face(0, -1, 0, minx, miny, minz, maxx, miny, minz, maxx, miny, maxz, minx, miny, maxz);
    }

    void add_quad(float x0, float y0, float z0,
                  float x1, float y1, float z1,
                  float x2, float y2, float z2,
                  float x3, float y3, float z3,
                  float nx, float ny, float nz,
                  float r, float g, float b) {
        if (!overlaps_xz(std::min({x0, x1, x2, x3}), std::max({x0, x1, x2, x3}),
                         std::min({z0, z1, z2, z3}), std::max({z0, z1, z2, z3}))) return;

        // Expand bounds
        auto expand = [&](float x, float y, float z) {
            min_x = std::min(min_x, x); max_x = std::max(max_x, x);
            min_y = std::min(min_y, y); max_y = std::max(max_y, y);
            min_z = std::min(min_z, z); max_z = std::max(max_z, z);
        };
        expand(x0, y0, z0); expand(x1, y1, z1);
        expand(x2, y2, z2); expand(x3, y3, z3);

        for (int v = 0; v < 6; ++v) {
            tri_colors.push_back(r);
            tri_colors.push_back(g);
            tri_colors.push_back(b);
            tri_normals.push_back(nx);
            tri_normals.push_back(ny);
            tri_normals.push_back(nz);
        }
        tri_positions.push_back(x0); tri_positions.push_back(y0); tri_positions.push_back(z0);
        tri_positions.push_back(x1); tri_positions.push_back(y1); tri_positions.push_back(z1);
        tri_positions.push_back(x2); tri_positions.push_back(y2); tri_positions.push_back(z2);
        tri_positions.push_back(x0); tri_positions.push_back(y0); tri_positions.push_back(z0);
        tri_positions.push_back(x2); tri_positions.push_back(y2); tri_positions.push_back(z2);
        tri_positions.push_back(x3); tri_positions.push_back(y3); tri_positions.push_back(z3);
    }

    void add_line(float x0, float y0, float z0,
                  float x1, float y1, float z1,
                  float r, float g, float b) {
        if (!overlaps_xz(std::min(x0, x1), std::max(x0, x1),
                         std::min(z0, z1), std::max(z0, z1))) return;

        expand_bounds(x0, y0, z0);
        expand_bounds(x1, y1, z1);
        line_positions.push_back(x0); line_positions.push_back(y0); line_positions.push_back(z0);
        line_positions.push_back(x1); line_positions.push_back(y1); line_positions.push_back(z1);
        line_colors.push_back(r); line_colors.push_back(g); line_colors.push_back(b);
        line_colors.push_back(r); line_colors.push_back(g); line_colors.push_back(b);
    }

    void expand_bounds(float x, float y, float z) {
        min_x = std::min(min_x, x); max_x = std::max(max_x, x);
        min_y = std::min(min_y, y); max_y = std::max(max_y, y);
        min_z = std::min(min_z, z); max_z = std::max(max_z, z);
    }

    void upload(MapCellVBO& cell) {
        cell.triangle_count = static_cast<int>(tri_positions.size() / 3);
        cell.line_count = static_cast<int>(line_positions.size() / 3);

        if (cell.triangle_count > 0) {
            glGenBuffers(1, &cell.vbo_positions);
            glBindBuffer(GL_ARRAY_BUFFER, cell.vbo_positions);
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(tri_positions.size() * sizeof(float)),
                         tri_positions.data(), GL_STATIC_DRAW);

            glGenBuffers(1, &cell.vbo_normals);
            glBindBuffer(GL_ARRAY_BUFFER, cell.vbo_normals);
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(tri_normals.size() * sizeof(float)),
                         tri_normals.data(), GL_STATIC_DRAW);

            glGenBuffers(1, &cell.vbo_colors);
            glBindBuffer(GL_ARRAY_BUFFER, cell.vbo_colors);
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(tri_colors.size() * sizeof(float)),
                         tri_colors.data(), GL_STATIC_DRAW);
        }

        if (cell.line_count > 0) {
            // Line data stored in same VBO slots — positions shared with tris
            // Actually, we store lines in the same positions VBO but we need to
            // track separate offsets. For simplicity, we'll store lines in the
            // vbo_positions/vbo_colors but note the line_count separately.
            // The positions VBO already has tri data; we append lines.
            // This means we need ONE combined positions VBO for tris+lines.
            // Let's handle this in upload by combining.
        }

        cell.bounds.min_x = min_x;
        cell.bounds.min_y = min_y;
        cell.bounds.min_z = min_z;
        cell.bounds.max_x = max_x;
        cell.bounds.max_y = max_y;
        cell.bounds.max_z = max_z;

        // Slightly inflate bounds to avoid precision issues
        const float eps = 0.1F;
        cell.bounds.min_x -= eps; cell.bounds.min_y -= eps; cell.bounds.min_z -= eps;
        cell.bounds.max_x += eps; cell.bounds.max_y += eps; cell.bounds.max_z += eps;
    }
};

// ============================================================
// Map assembly
// ============================================================

void build_arena(std::array<CellBuilder, MapGeometry::kTotalCells>& builders) {
    // ====== Javelin-4 inspired Crucible arena ======

    // --- Arena floor ---
    {
        float r = 0.10F, g = 0.13F, b = 0.18F;
        for (auto& builder : builders) {
            builder.add_box(-15.0F, -0.05F, -15.0F, 15.0F, 0.0F, 15.0F, r, g, b);
        }
    }

    // --- Outer ring track ---
    {
        float r = 0.14F, g = 0.17F, b = 0.22F;
        for (auto& builder : builders) {
            builder.add_box(-8.0F, 0.0F, -8.0F, 8.0F, 0.05F, 8.0F, r, g, b);
        }
    }

    // --- Central platform ---
    {
        float r = 0.22F, g = 0.28F, b = 0.36F;
        for (auto& builder : builders) {
            builder.add_box(-4.0F, 0.0F, -4.0F, 4.0F, 1.5F, 4.0F, r, g, b);
        }
    }

    // --- Central pillar ---
    {
        float r = 0.26F, g = 0.32F, b = 0.40F;
        for (auto& builder : builders) {
            builder.add_box(-0.8F, 1.5F, -0.8F, 0.8F, 3.5F, 0.8F, r, g, b);
        }
    }

    // --- 4 Ramps (N/S/E/W) ---
    // We add ramps to all cells since they're near center and span multiple
    {
        float r = 0.18F, g = 0.24F, b = 0.32F;
        for (auto& builder : builders) {
            builder.add_quad(-1.5F, 0.05F, 4.0F, 1.5F, 0.05F, 4.0F,
                             1.5F, 1.5F, 4.0F, -1.5F, 1.5F, 4.0F,
                             0, 0.7F, 0.7F, r, g, b);  // North
        }
        {
            float r = 0.17F, g = 0.23F, b = 0.31F;
            for (auto& builder : builders) {
                builder.add_quad(-1.5F, 0.05F, -4.0F, 1.5F, 0.05F, -4.0F,
                                 1.5F, 1.5F, -4.0F, -1.5F, 1.5F, -4.0F,
                                 0, 0.7F, -0.7F, r, g, b);  // South
            }
        }
        {
            float r = 0.17F, g = 0.23F, b = 0.31F;
            for (auto& builder : builders) {
                builder.add_quad(4.0F, 0.05F, -1.5F, 4.0F, 0.05F, 1.5F,
                                 4.0F, 1.5F, 1.5F, 4.0F, 1.5F, -1.5F,
                                 0.7F, 0.7F, 0, r, g, b);  // East
            }
        }
        {
            float r = 0.17F, g = 0.23F, b = 0.31F;
            for (auto& builder : builders) {
                builder.add_quad(-4.0F, 0.05F, -1.5F, -4.0F, 0.05F, 1.5F,
                                 -4.0F, 1.5F, 1.5F, -4.0F, 1.5F, -1.5F,
                                 -0.7F, 0.7F, 0, r, g, b);  // West
            }
        }
    }

    // --- Cover blocks on central platform (4 pillars) ---
    {
        float r = 0.28F, g = 0.34F, b = 0.42F;
        for (auto& builder : builders) {
            builder.add_box(2.0F, 1.5F, 0.6F, 3.2F, 2.2F, 1.4F, r, g, b);
            builder.add_box(-3.2F, 1.5F, 0.6F, -2.0F, 2.2F, 1.4F, r, g, b);
            builder.add_box(2.0F, 1.5F, -1.4F, 3.2F, 2.2F, -0.6F, r, g, b);
            builder.add_box(-3.2F, 1.5F, -1.4F, -2.0F, 2.2F, -0.6F, r, g, b);
        }
    }

    // --- 4 Connector bridges ---
    {
        float br = 0.16F, bg = 0.32F, bb = 0.28F;
        float sr = 0.13F, sg = 0.26F, sb = 0.22F;

        auto add_bridge = [&](float cx, float cz, float dx, float dz) {
            float hw = 1.0F, y = 1.0F, thick = 0.15F, len = 5.0F;
            float nx = -dz / len * hw, nz = dx / len * hw;
            for (auto& builder : builders) {
                builder.add_quad(
                    cx + dx*2.0F - nx, y + thick, cz + dz*2.0F - nz,
                    cx + dx*2.0F + nx, y + thick, cz + dz*2.0F + nz,
                    cx + dx*7.0F + nx, y + thick, cz + dz*7.0F + nz,
                    cx + dx*7.0F - nx, y + thick, cz + dz*7.0F - nz,
                    0, 1, 0, br, bg, bb);
                builder.add_box(cx + dx*4.0F - 0.2F, 0.0F, cz + dz*4.0F - 0.2F,
                               cx + dx*4.0F + 0.2F, y, cz + dz*4.0F + 0.2F,
                               sr, sg, sb);
            }
        };
        add_bridge(4.0F, 4.0F, 0.707F, 0.707F);
        add_bridge(-4.0F, 4.0F, -0.707F, 0.707F);
        add_bridge(4.0F, -4.0F, 0.707F, -0.707F);
        add_bridge(-4.0F, -4.0F, -0.707F, -0.707F);
    }

    // --- Alpha Spawn (West) ---
    {
        float r1 = 0.18F, g1 = 0.23F, b1 = 0.30F;
        float r2 = 0.24F, g2 = 0.30F, b2 = 0.36F;
        for (auto& builder : builders) {
            builder.add_box(-13.0F, 0.0F, -3.0F, -10.0F, 0.3F, 3.0F, r1, g1, b1);
            builder.add_box(-13.0F, 0.3F, 0.0F, -11.0F, 1.5F, 1.5F, r2, g2, b2);
            builder.add_box(-13.0F, 0.3F, -1.5F, -11.0F, 1.5F, 0.0F, r2, g2, b2);
        }
    }

    // --- Bravo Spawn (East) ---
    {
        float r1 = 0.18F, g1 = 0.23F, b1 = 0.30F;
        float r2 = 0.24F, g2 = 0.30F, b2 = 0.36F;
        for (auto& builder : builders) {
            builder.add_box(10.0F, 0.0F, -3.0F, 13.0F, 0.3F, 3.0F, r1, g1, b1);
            builder.add_box(11.0F, 0.3F, 0.0F, 13.0F, 1.5F, 1.5F, r2, g2, b2);
            builder.add_box(11.0F, 0.3F, -1.5F, 13.0F, 1.5F, 0.0F, r2, g2, b2);
        }
    }

    // --- Heavy alcoves (North/South) ---
    {
        float r1 = 0.20F, g1 = 0.24F, b1 = 0.28F;
        float r2 = 0.26F, g2 = 0.30F, b2 = 0.36F;
        for (auto& builder : builders) {
            builder.add_box(-2.0F, 0.0F, 8.0F, 2.0F, 0.3F, 9.5F, r1, g1, b1);
            builder.add_box(-2.0F, 0.0F, -9.5F, 2.0F, 0.3F, -8.0F, r1, g1, b1);
            builder.add_box(-1.0F, 0.3F, 8.5F, 1.0F, 1.0F, 9.2F, r2, g2, b2);
            builder.add_box(-1.0F, 0.3F, -9.2F, 1.0F, 1.0F, -8.5F, r2, g2, b2);
        }
    }

    // --- Side route platforms ---
    {
        float r = 0.17F, g = 0.27F, b = 0.25F;
        for (auto& builder : builders) {
            builder.add_box(5.0F, 0.8F, 6.0F, 7.0F, 1.0F, 8.0F, r, g, b);
            builder.add_box(-7.0F, 0.8F, 6.0F, -5.0F, 1.0F, 8.0F, r, g, b);
            builder.add_box(5.0F, 0.8F, -8.0F, 7.0F, 1.0F, -6.0F, r, g, b);
            builder.add_box(-7.0F, 0.8F, -8.0F, -5.0F, 1.0F, -6.0F, r, g, b);
        }
    }

    // --- Scattered cover blocks on outer ring ---
    {
        float r = 0.28F, g = 0.34F, b = 0.38F;
        for (auto& builder : builders) {
            builder.add_box(5.5F, 0.0F, 2.5F, 6.5F, 0.9F, 3.5F, r, g, b);
            builder.add_box(-6.5F, 0.0F, 2.5F, -5.5F, 0.9F, 3.5F, r, g, b);
            builder.add_box(5.5F, 0.0F, -3.5F, 6.5F, 0.9F, -2.5F, r, g, b);
            builder.add_box(-6.5F, 0.0F, -3.5F, -5.5F, 0.9F, -2.5F, r, g, b);
            builder.add_box(2.5F, 0.0F, 5.5F, 3.5F, 0.9F, 6.5F, r, g, b);
            builder.add_box(-3.5F, 0.0F, 5.5F, -2.5F, 0.9F, 6.5F, r, g, b);
            builder.add_box(2.5F, 0.0F, -6.5F, 3.5F, 0.9F, -5.5F, r, g, b);
            builder.add_box(-3.5F, 0.0F, -6.5F, -2.5F, 0.9F, -5.5F, r, g, b);
        }
    }

    // --- Low boundary walls ---
    {
        float r = 0.20F, g = 0.24F, b = 0.30F;
        for (auto& builder : builders) {
            builder.add_box(-14.0F, 0.0F, -14.2F, 14.0F, 0.4F, -13.8F, r, g, b);
            builder.add_box(-14.0F, 0.0F, 13.8F, 14.0F, 0.4F, 14.2F, r, g, b);
            builder.add_box(-14.2F, 0.0F, -14.0F, -13.8F, 0.4F, 14.0F, r, g, b);
            builder.add_box(13.8F, 0.0F, -14.0F, 14.2F, 0.4F, 14.0F, r, g, b);
        }
    }

    // --- Direction markers (lines) — Red (North) ---
    {
        float r = 0.8F, g = 0.2F, b = 0.2F;
        for (auto& builder : builders) {
            builder.add_line(0.0F, 0.06F, 8.5F, 0.0F, 0.06F, 11.0F, r, g, b);
            builder.add_line(-0.5F, 0.06F, 10.5F, 0.0F, 0.06F, 11.0F, r, g, b);
            builder.add_line(0.5F, 0.06F, 10.5F, 0.0F, 0.06F, 11.0F, r, g, b);
        }
    }
    // --- Direction markers — Blue (South) ---
    {
        float r = 0.2F, g = 0.2F, b = 0.8F;
        for (auto& builder : builders) {
            builder.add_line(0.0F, 0.06F, -8.5F, 0.0F, 0.06F, -11.0F, r, g, b);
            builder.add_line(-0.5F, 0.06F, -10.5F, 0.0F, 0.06F, -11.0F, r, g, b);
            builder.add_line(0.5F, 0.06F, -10.5F, 0.0F, 0.06F, -11.0F, r, g, b);
        }
    }
}

} // namespace

// ============================================================
// MapGeometry
// ============================================================

MapGeometry::~MapGeometry() {
    destroy();
}

int MapGeometry::cell_index(float world_x, float world_z) const {
    float cell_w = (world_max_x - world_min_x) / static_cast<float>(kGridSize);
    float cell_h = (world_max_z - world_min_z) / static_cast<float>(kGridSize);
    int cx = static_cast<int>((world_x - world_min_x) / cell_w);
    int cz = static_cast<int>((world_z - world_min_z) / cell_h);
    cx = std::max(0, std::min(cx, kGridSize - 1));
    cz = std::max(0, std::min(cz, kGridSize - 1));
    return cz * kGridSize + cx;
}

void MapGeometry::build() {
    std::array<CellBuilder, kTotalCells> builders;

    // Set up cell bounds
    float cell_w = (world_max_x - world_min_x) / static_cast<float>(kGridSize);
    float cell_h = (world_max_z - world_min_z) / static_cast<float>(kGridSize);
    for (int cz = 0; cz < kGridSize; ++cz) {
        for (int cx = 0; cx < kGridSize; ++cx) {
            int idx = cz * kGridSize + cx;
            auto& b = builders[static_cast<std::size_t>(idx)];
            b.cell_min_x = world_min_x + static_cast<float>(cx) * cell_w;
            b.cell_max_x = world_min_x + static_cast<float>(cx + 1) * cell_w;
            b.cell_min_z = world_min_z + static_cast<float>(cz) * cell_h;
            b.cell_max_z = world_min_z + static_cast<float>(cz + 1) * cell_h;
        }
    }

    // Build all geometry into all cells (each cell gets a full copy for now —
    // this gives us spatial partitioning without complex clipping)
    build_arena(builders);

    // Upload to GPU
    for (int i = 0; i < kTotalCells; ++i) {
        builders[static_cast<std::size_t>(i)].upload(cells[static_cast<std::size_t>(i)]);
    }

    // Handle combined positions VBO for lines: lines are appended after tris
    // so we need to re-upload. This is handled in the render path.
    // For now, the line data is stored interleaved in tri_positions.
    // We'll fix this by having a separate combined upload.
    // Actually, let's handle lines separately in the VBO system.

    // Re-upload with combined positions (tris first, then lines)
    for (int i = 0; i < kTotalCells; ++i) {
        auto& builder = builders[static_cast<std::size_t>(i)];
        auto& cell = cells[static_cast<std::size_t>(i)];

        if (cell.triangle_count > 0 || cell.line_count > 0) {
            // Combine tri + line positions into one VBO
            std::vector<float> all_positions;
            all_positions.reserve(builder.tri_positions.size() + builder.line_positions.size());
            all_positions.insert(all_positions.end(),
                                 builder.tri_positions.begin(), builder.tri_positions.end());
            all_positions.insert(all_positions.end(),
                                 builder.line_positions.begin(), builder.line_positions.end());

            std::vector<float> all_colors;
            all_colors.reserve(builder.tri_colors.size() + builder.line_colors.size());
            all_colors.insert(all_colors.end(),
                              builder.tri_colors.begin(), builder.tri_colors.end());
            all_colors.insert(all_colors.end(),
                              builder.line_colors.begin(), builder.line_colors.end());

            // Normals for lines (zero — they'll be ignored for line rendering)
            std::vector<float> all_normals;
            all_normals.reserve(builder.tri_normals.size() + builder.line_positions.size());
            all_normals.insert(all_normals.end(),
                               builder.tri_normals.begin(), builder.tri_normals.end());
            all_normals.resize(all_positions.size(), 0.0F);

            glGenBuffers(1, &cell.vbo_positions);
            glBindBuffer(GL_ARRAY_BUFFER, cell.vbo_positions);
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(all_positions.size() * sizeof(float)),
                         all_positions.data(), GL_STATIC_DRAW);

            glGenBuffers(1, &cell.vbo_normals);
            glBindBuffer(GL_ARRAY_BUFFER, cell.vbo_normals);
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(all_normals.size() * sizeof(float)),
                         all_normals.data(), GL_STATIC_DRAW);

            glGenBuffers(1, &cell.vbo_colors);
            glBindBuffer(GL_ARRAY_BUFFER, cell.vbo_colors);
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(all_colors.size() * sizeof(float)),
                         all_colors.data(), GL_STATIC_DRAW);
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void MapGeometry::destroy() {
    for (auto& cell : cells) {
        if (cell.vbo_positions) glDeleteBuffers(1, &cell.vbo_positions);
        if (cell.vbo_normals)  glDeleteBuffers(1, &cell.vbo_normals);
        if (cell.vbo_colors)   glDeleteBuffers(1, &cell.vbo_colors);
        cell = {};
    }
}

void MapGeometry::collect_visible(const Frustum& frustum,
                                   int* out_indices, int& out_count) const {
    out_count = 0;
    for (int i = 0; i < kTotalCells; ++i) {
        if (cells[static_cast<std::size_t>(i)].has_geometry() &&
            frustum.intersects_aabb(cells[static_cast<std::size_t>(i)].bounds)) {
            out_indices[out_count++] = i;
        }
    }
}

} // namespace ae::render
