#pragma once

#include "ae/render/frustum.h"

#include <array>
#include <cstddef>
#include <vector>

namespace ae::render {

/**
 * @brief A single grid cell's worth of pre-built map geometry.
 *
 * All vertex data is non-indexed (hard edges require per-face normals).
 * Solid geometry (boxes, ramps, bridges) uses GL_TRIANGLES.
 * Line geometry (direction markers) uses GL_LINES.
 */
struct MapCellVBO {
    unsigned int vbo_positions {0};
    unsigned int vbo_normals  {0};
    unsigned int vbo_colors   {0};
    int triangle_count {0};   // vertices for GL_TRIANGLES
    int line_count      {0};   // vertices for GL_LINES

    // Bounding box for frustum culling
    AABB bounds;

    bool has_geometry() const { return triangle_count > 0 || line_count > 0; }
};

/**
 * @brief Pre-built, spatially-partitioned arena geometry.
 *
 * The arena is split into a kGridSize × kGridSize grid of cells.
 * At init time, all map geometry is built into triangle/line VBOs
 * and bucketed into the cell(s) each piece overlaps.
 *
 * At render time, visible cells are collected and drawn with
 * a single glMultiDrawArrays call per geometry type.
 */
struct MapGeometry {
    static constexpr int kGridSize = 4;  // 4×4 = 16 cells
    static constexpr int kTotalCells = kGridSize * kGridSize;

    // World-space extents of the grid
    float world_min_x {-16.0F};
    float world_min_z {-16.0F};
    float world_max_x {16.0F};
    float world_max_z {16.0F};

    std::array<MapCellVBO, kTotalCells> cells;

    MapGeometry() = default;
    ~MapGeometry();

    MapGeometry(const MapGeometry&) = delete;
    MapGeometry& operator=(const MapGeometry&) = delete;
    MapGeometry(MapGeometry&&) = delete;
    MapGeometry& operator=(MapGeometry&&) = delete;

    /** Build all arena geometry and upload to GPU. */
    void build();

    /** Free all GPU resources. */
    void destroy();

    /**
     * @brief Get the cell index for a world position.
     */
    int cell_index(float world_x, float world_z) const;

    /**
     * @brief Get cells whose bounding boxes intersect the frustum.
     * @param frustum     The view frustum.
     * @param out_indices Output: visible cell indices.
     * @param out_count   Output: number of visible cells.
     */
    void collect_visible(const Frustum& frustum,
                         int* out_indices, int& out_count) const;
};

} // namespace ae::render
