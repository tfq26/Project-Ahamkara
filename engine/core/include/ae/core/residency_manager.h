#pragma once

// Async region-based streaming and residency management.
//
// Divides the world's x/z plane into a grid of rectangular regions that align
// with ae::render::SpatialGrid cells.  The ResidencyManager tracks which
// regions are "resident" (should be loaded) given a player/camera position and
// a load radius.  It produces explicit load/unload transitions as the player
// moves between regions, enabling an async caller (e.g. World or a streaming
// system) to schedule content loading and unloading without blocking.
//
// Pure / dependency-light: standard library + ae::core types only.
// Deterministic and testable: all state is explicit; no threads or callbacks.
//
// Future: a hierarchical region tree (e.g. quadtree) can replace the flat grid
// when world scale grows beyond a few hundred regions.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "ae/core/types.h"

namespace ae::core {

/// 2D coordinate of a region in the residency grid.
/// Matches the cell coordinate convention used by SpatialGrid.
struct RegionCoord {
    int cx {0};
    int cy {0};
};

inline bool operator==(RegionCoord a, RegionCoord b) noexcept { return a.cx == b.cx && a.cy == b.cy; }
inline bool operator!=(RegionCoord a, RegionCoord b) noexcept { return !(a == b); }
inline bool operator<(RegionCoord a, RegionCoord b) noexcept {
    return (a.cy < b.cy) || (a.cy == b.cy && a.cx < b.cx);
}

/// A single load or unload transition for a region boundary crossing.
/// Consumers (e.g. a streaming system) consume these via consume_pending()
/// to schedule async load/unload of region content.
struct RegionTransition {
    RegionCoord coord {};
    bool load {true};  // true = load, false = unload
};

/// Manages region-based residency: which regions of the world are currently
/// "resident" (loaded) and which should be loaded/unloaded as the player moves.
///
/// Usage:
///   1. init(cols, rows, load_radius) with world dimensions.
///   2. Each frame/tick, call update(player_x, player_z, cell_size, ox, oz).
///   3. Call consume_pending() to retrieve and clear transitions.
///   4. Query is_resident(cx, cy) to check individual region state.
///
/// The load radius is in *region* units (not world units).  A load radius of 1
/// means the player's current region plus its immediate 4‑neighbors are
/// resident.  Radius 0 means only the player's own region.
class ResidencyManager {
public:
    ResidencyManager() = default;

    /// Initialize the grid.  `cols` and `rows` must be >= 1 (clamped).
    /// `load_radius` is the number of adjacent regions (in Chebyshev distance)
    /// that should be kept loaded around the player's current region.
    void init(int cols, int rows, int load_radius = 1) {
        cols_ = (cols > 0) ? cols : 1;
        rows_ = (rows > 0) ? rows : 1;
        load_radius_ = (load_radius >= 0) ? load_radius : 1;
        const auto count = static_cast<std::size_t>(cols_) * static_cast<std::size_t>(rows_);
        resident_.assign(count, 0);
        desired_.assign(count, 0);
    }

    // -- Accessors --

    [[nodiscard]] int cols() const noexcept { return cols_; }
    [[nodiscard]] int rows() const noexcept { return rows_; }
    [[nodiscard]] int load_radius() const noexcept { return load_radius_; }
    [[nodiscard]] bool empty() const noexcept { return resident_.empty(); }
    [[nodiscard]] std::size_t region_count() const noexcept { return resident_.size(); }

    /// Number of currently-resident regions.
    [[nodiscard]] int resident_count() const noexcept {
        int n = 0;
        for (auto flag : resident_) {
            if (flag != 0) ++n;
        }
        return n;
    }

    /// Query whether a specific region is currently resident.
    [[nodiscard]] bool is_resident(int cx, int cy) const noexcept {
        if (!in_bounds(cx, cy)) return false;
        return resident_[index(cx, cy)] != 0;
    }

    /// Current player region (the region the player is standing in).
    [[nodiscard]] RegionCoord player_region() const noexcept { return player_region_; }

    // -- Update --

    /// Update residency state based on a world-space position.
    /// `cell_size` is the world-space size of one region/cell.
    /// `origin_x`/`origin_z` is the world-space origin of the grid.
    ///
    /// This computes which regions *should* be resident (desired set) and diffs
    /// against the current resident set.  Transitions are appended to an
    /// internal pending list, which the caller retrieves with
    /// consume_pending().
    ///
    /// Call this after init() and before consuming transitions each frame/tick.
    void update(float world_x, float world_z, float cell_size, float origin_x, float origin_z) {
        if (resident_.empty()) return;

        // Clamp cell size to avoid division by zero.
        const float cs = (cell_size > 0.0F) ? cell_size : 1.0F;

        // Determine the player's current region.
        const int pcx = clamp_to_grid(static_cast<int>((world_x - origin_x) / cs), cols_);
        const int pcy = clamp_to_grid(static_cast<int>((world_z - origin_z) / cs), rows_);
        player_region_ = {pcx, pcy};

        // Compute desired residency: mark regions within load_radius Chebyshev
        // distance of the player's region.
        for (std::size_t i = 0; i < desired_.size(); ++i) {
            desired_[i] = 0;
        }

        const int r0 = std::max(0, pcx - load_radius_);
        const int r1 = std::min(cols_ - 1, pcx + load_radius_);
        const int c0 = std::max(0, pcy - load_radius_);
        const int c1 = std::min(rows_ - 1, pcy + load_radius_);

        for (int cy = c0; cy <= c1; ++cy) {
            for (int cx = r0; cx <= r1; ++cx) {
                desired_[index(cx, cy)] = 1;
            }
        }

        // Diff: produce transitions for regions whose state changed.
        for (std::size_t i = 0; i < resident_.size(); ++i) {
            if (resident_[i] != desired_[i]) {
                const int cx = static_cast<int>(i % static_cast<std::size_t>(cols_));
                const int cy = static_cast<int>(i / static_cast<std::size_t>(cols_));
                pending_.push_back({RegionCoord{cx, cy}, desired_[i] != 0});
                resident_[i] = desired_[i];
            }
        }
    }

    // -- Transition consumption --

    /// Retrieve and clear the list of pending load/unload transitions.
    [[nodiscard]] std::vector<RegionTransition> consume_pending() noexcept {
        std::vector<RegionTransition> result;
        result.swap(pending_);
        return result;
    }

    /// Peek at pending transitions without consuming them.
    [[nodiscard]] const std::vector<RegionTransition>& pending_transitions() const noexcept {
        return pending_;
    }

    /// Number of pending transitions.
    [[nodiscard]] std::size_t pending_count() const noexcept { return pending_.size(); }

private:
    [[nodiscard]] bool in_bounds(int cx, int cy) const noexcept {
        return cx >= 0 && cy >= 0 && cx < cols_ && cy < rows_;
    }

    [[nodiscard]] std::size_t index(int cx, int cy) const noexcept {
        return static_cast<std::size_t>(cy) * static_cast<std::size_t>(cols_) +
               static_cast<std::size_t>(cx);
    }

    [[nodiscard]] static int clamp_to_grid(int value, int limit) noexcept {
        if (value < 0) return 0;
        if (value >= limit) return limit - 1;
        return value;
    }

    int cols_ {1};
    int rows_ {1};
    int load_radius_ {1};
    RegionCoord player_region_ {};
    std::vector<unsigned char> resident_;
    std::vector<unsigned char> desired_;
    std::vector<RegionTransition> pending_;
};

}  // namespace ae::core
