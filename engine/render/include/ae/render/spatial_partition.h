#pragma once

// Uniform 2D spatial partition for world-scale culling.
//
// Divides the x/z plane into square cells of a fixed world-space size. Each
// cell tracks instance/entity IDs that overlap it. Supports frustum culling
// via ae::render::Frustum and AABB queries.
//
// Pure / GL-free; no rendering or physics dependencies.
//
// Future: hierarchical grids (linked cells, quadtree, or dynamic-resize)
// can replace this when world scale grows beyond a few hundred cells.

#include "ae/render/frustum.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace ae::render {

/// A rect in the x/z plane (world space).
struct CellRect {
    int min_cx {0};
    int min_cy {0};
    int max_cx {0};
    int max_cy {0};
};

/// A uniform 2D spatial partition. Grid cells contain instance/entity IDs.
/// Out-of-bounds queries return empty results (no assertions).
///
/// Thread-compatible: all methods are const (query) or write cells under a
/// consistent convention. Not thread-safe for concurrent read+write.
class SpatialGrid {
public:
    SpatialGrid() = default;

    /// Construct a grid that covers world-space [ox, ox + cols*cs) x
    /// [oz, oz + rows*cs).  Each cell is `cell_size` world units square.
    SpatialGrid(int cols, int rows, float cell_size,
                float origin_x, float origin_z)
        : cols_(cols > 0 ? cols : 1),
          rows_(rows > 0 ? rows : 1),
          cell_size_(cell_size > 0.0F ? cell_size : 1.0F),
          origin_x_(origin_x),
          origin_z_(origin_z),
          cells_(static_cast<std::size_t>(cols_) * static_cast<std::size_t>(rows_))
    {}

    // -- Accessors --

    [[nodiscard]] int cols() const { return cols_; }
    [[nodiscard]] int rows() const { return rows_; }
    [[nodiscard]] float cell_size() const { return cell_size_; }
    [[nodiscard]] float origin_x() const { return origin_x_; }
    [[nodiscard]] float origin_z() const { return origin_z_; }

    [[nodiscard]] bool empty() const { return cells_.empty(); }
    [[nodiscard]] std::size_t cell_count() const { return cells_.size(); }

    /// Number of IDs stored in a specific cell.
    [[nodiscard]] std::size_t cell_size(int cx, int cy) const {
        if (!in_bounds(cx, cy)) return 0;
        return cells_[index(cx, cy)].size();
    }

    /// Total number of IDs across all cells.
    [[nodiscard]] std::size_t total_ids() const {
        std::size_t n = 0;
        for (const auto& c : cells_) n += c.size();
        return n;
    }

    // -- Mutation --

    /// Add an ID to every cell its AABB overlaps (inclusive of both min/max).
    /// The AABB is in world-space coordinates.
    void insert(std::uint64_t id, const AABB& world_aabb) {
        CellRect r = aabb_to_cells(world_aabb);
        for (int cy = r.min_cy; cy <= r.max_cy; ++cy) {
            for (int cx = r.min_cx; cx <= r.max_cx; ++cx) {
                if (in_bounds(cx, cy)) {
                    cells_[index(cx, cy)].push_back(id);
                }
            }
        }
    }

    /// Remove one occurrence of `id` from each cell its AABB overlaps.
    /// Does nothing if the ID is not found (linear scan per cell).
    void remove(std::uint64_t id, const AABB& world_aabb) {
        CellRect r = aabb_to_cells(world_aabb);
        for (int cy = r.min_cy; cy <= r.max_cy; ++cy) {
            for (int cx = r.min_cx; cx <= r.max_cx; ++cx) {
                if (in_bounds(cx, cy)) {
                    auto& cell = cells_[index(cx, cy)];
                    auto it = std::find(cell.begin(), cell.end(), id);
                    if (it != cell.end()) {
                        *it = cell.back();
                        cell.pop_back();
                    }
                }
            }
        }
    }

    /// Remove all IDs from every cell (does not resize).
    void clear() {
        for (auto& c : cells_) c.clear();
    }

    // -- Queries --

    /// Collect unique IDs from cells that intersect the AABB.
    /// `out` is appended to; caller should ensure capacity or clear first.
    void query_aabb(const AABB& world_aabb, std::vector<std::uint64_t>& out) const {
        CellRect r = aabb_to_cells(world_aabb);
        for (int cy = r.min_cy; cy <= r.max_cy; ++cy) {
            for (int cx = r.min_cx; cx <= r.max_cx; ++cx) {
                if (in_bounds(cx, cy)) {
                    const auto& cell = cells_[index(cx, cy)];
                    out.insert(out.end(), cell.begin(), cell.end());
                }
            }
        }
    }

    /// Collect unique IDs from cells that intersect the view frustum (2D
    /// projection: the AABB of each cell is tested against the frustum).
    void query_frustum(const Frustum& frustum, std::vector<std::uint64_t>& out) const {
        for (int cy = 0; cy < rows_; ++cy) {
            for (int cx = 0; cx < cols_; ++cx) {
                AABB cell_aabb = cell_world_aabb(cx, cy);
                if (frustum.intersects_aabb(cell_aabb)) {
                    const auto& cell = cells_[index(cx, cy)];
                    out.insert(out.end(), cell.begin(), cell.end());
                }
            }
        }
    }

    /// Return the list of IDs in a single cell (const ref). Empty vector for
    /// out-of-bounds coordinates.
    [[nodiscard]] const std::vector<std::uint64_t>& cell_ids(int cx, int cy) const {
        static const std::vector<std::uint64_t> kEmpty;
        if (!in_bounds(cx, cy)) return kEmpty;
        return cells_[index(cx, cy)];
    }

    /// Convert a world-space position to cell coordinates.  Clamped to grid.
    [[nodiscard]] int world_to_cx(float wx) const {
        int c = static_cast<int>((wx - origin_x_) / cell_size_);
        return std::clamp(c, 0, cols_ - 1);
    }
    [[nodiscard]] int world_to_cy(float wz) const {
        int c = static_cast<int>((wz - origin_z_) / cell_size_);
        return std::clamp(c, 0, rows_ - 1);
    }

    /// World-space AABB of a single cell.
    [[nodiscard]] AABB cell_world_aabb(int cx, int cy) const {
        if (!in_bounds(cx, cy)) return {};
        const float wx0 = origin_x_ + static_cast<float>(cx) * cell_size_;
        const float wz0 = origin_z_ + static_cast<float>(cy) * cell_size_;
        const float wx1 = wx0 + cell_size_;
        const float wz1 = wz0 + cell_size_;
        // Use a large Y range so frustum intersection treats the cell as a
        // vertical column from -1e6 to +1e6 (infinite-height column).
        constexpr float kBigY = 1.0e6F;
        return {wx0, -kBigY, wz0, wx1, kBigY, wz1};
    }

    /// World-space AABB of the entire grid.
    [[nodiscard]] AABB grid_world_aabb() const {
        return {origin_x_, -1.0e6F, origin_z_,
                origin_x_ + static_cast<float>(cols_) * cell_size_,
                1.0e6F,
                origin_z_ + static_cast<float>(rows_) * cell_size_};
    }

private:
    [[nodiscard]] bool in_bounds(int cx, int cy) const {
        return cx >= 0 && cy >= 0 && cx < cols_ && cy < rows_;
    }
    [[nodiscard]] std::size_t index(int cx, int cy) const {
        return static_cast<std::size_t>(cy) * static_cast<std::size_t>(cols_) +
               static_cast<std::size_t>(cx);
    }

    /// Convert a world-space AABB to a cell rect (clamped to grid bounds).
    /// Uses a small epsilon on the max bounds so an AABB exactly on a cell
    /// boundary does not overflow into the next cell.
    [[nodiscard]] CellRect aabb_to_cells(const AABB& aabb) const {
        constexpr float kEps = 1.0e-6F;
        CellRect r;
        r.min_cx = std::max(0, static_cast<int>((aabb.min_x - origin_x_) / cell_size_));
        r.min_cy = std::max(0, static_cast<int>((aabb.min_z - origin_z_) / cell_size_));
        r.max_cx = std::min(cols_ - 1, static_cast<int>((aabb.max_x - origin_x_ - kEps) / cell_size_));
        r.max_cy = std::min(rows_ - 1, static_cast<int>((aabb.max_z - origin_z_ - kEps) / cell_size_));
        if (r.min_cx > r.max_cx) { int t = r.min_cx; r.min_cx = r.max_cx; r.max_cx = t; }
        if (r.min_cy > r.max_cy) { int t = r.min_cy; r.min_cy = r.max_cy; r.max_cy = t; }
        return r;
    }

    int cols_ {1};
    int rows_ {1};
    float cell_size_ {1.0F};
    float origin_x_ {0.0F};
    float origin_z_ {0.0F};
    std::vector<std::vector<std::uint64_t>> cells_;
};

}  // namespace ae::render
