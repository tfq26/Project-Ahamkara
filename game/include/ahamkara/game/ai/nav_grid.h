#pragma once

// Uniform 2D navigation grid + deterministic A* pathfinding.
//
// Dependency-free (standard library only) and pure: no rendering, networking,
// or physics. This is the first slice of FPS AI navigation; later work can add
// a real navmesh, dynamic obstacles, and steering on top of (or replacing) this.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <vector>

namespace ahamkara::game::ai {

struct GridCoord {
    int x {0};
    int y {0};
};

inline bool operator==(GridCoord a, GridCoord b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(GridCoord a, GridCoord b) { return !(a == b); }

/// A uniform grid of walkable/blocked cells. Out-of-bounds cells are blocked.
class NavGrid {
public:
    NavGrid(int width, int height)
        : width_(width > 0 ? width : 0),
          height_(height > 0 ? height : 0),
          blocked_(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), 0) {}

    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }

    [[nodiscard]] bool in_bounds(GridCoord c) const {
        return c.x >= 0 && c.y >= 0 && c.x < width_ && c.y < height_;
    }

    void set_blocked(GridCoord c, bool blocked) {
        if (in_bounds(c)) blocked_[index(c)] = blocked ? 1U : 0U;
    }

    [[nodiscard]] bool is_blocked(GridCoord c) const {
        return !in_bounds(c) || blocked_[index(c)] != 0U;
    }

    [[nodiscard]] bool is_walkable(GridCoord c) const {
        return in_bounds(c) && blocked_[index(c)] == 0U;
    }

private:
    [[nodiscard]] std::size_t index(GridCoord c) const {
        return static_cast<std::size_t>(c.y) * static_cast<std::size_t>(width_) +
               static_cast<std::size_t>(c.x);
    }

    int width_;
    int height_;
    std::vector<std::uint8_t> blocked_;
};

/// A* shortest path from `start` to `goal` (both inclusive). Returns an empty
/// vector if either endpoint is unwalkable or no path exists. With
/// `allow_diagonal`, uses 8-connectivity (cost sqrt(2)) and forbids cutting
/// corners between two blocked orthogonal cells.
///
/// Deterministic: open-set ties break by insertion order and neighbors are
/// expanded in a fixed order, so the same input always yields the same path.
[[nodiscard]] inline std::vector<GridCoord> find_path(
    const NavGrid& grid, GridCoord start, GridCoord goal, bool allow_diagonal = false) {
    std::vector<GridCoord> path;
    if (!grid.is_walkable(start) || !grid.is_walkable(goal)) {
        return path;
    }

    const int width = grid.width();
    const auto cell_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(grid.height());
    const auto to_index = [width](GridCoord c) {
        return static_cast<std::size_t>(c.y) * static_cast<std::size_t>(width) +
               static_cast<std::size_t>(c.x);
    };
    const auto to_coord = [width](std::size_t i) {
        return GridCoord{static_cast<int>(i % static_cast<std::size_t>(width)),
                         static_cast<int>(i / static_cast<std::size_t>(width))};
    };

    constexpr float kInf = std::numeric_limits<float>::infinity();
    constexpr float kSqrt2 = 1.41421356F;
    std::vector<float> g_score(cell_count, kInf);
    std::vector<int> came_from(cell_count, -1);
    std::vector<char> closed(cell_count, 0);

    const auto heuristic = [allow_diagonal](GridCoord a, GridCoord b) -> float {
        const int dx = std::abs(a.x - b.x);
        const int dy = std::abs(a.y - b.y);
        if (allow_diagonal) {
            const int dmin = std::min(dx, dy);
            const int dmax = std::max(dx, dy);
            return static_cast<float>(dmax - dmin) + kSqrt2 * static_cast<float>(dmin);
        }
        return static_cast<float>(dx + dy);
    };

    struct Node {
        float f;
        std::uint64_t order;
        std::size_t cell;
    };
    struct Cmp {
        bool operator()(const Node& a, const Node& b) const {
            return a.f > b.f || (a.f == b.f && a.order > b.order);
        }
    };
    std::priority_queue<Node, std::vector<Node>, Cmp> open;

    std::uint64_t counter = 0;
    const std::size_t start_index = to_index(start);
    const std::size_t goal_index = to_index(goal);
    g_score[start_index] = 0.0F;
    open.push({heuristic(start, goal), counter++, start_index});

    static const int kOrtho[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    static const int kDiag[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

    bool found = false;
    while (!open.empty()) {
        const Node current = open.top();
        open.pop();
        if (closed[current.cell] != 0) continue;
        closed[current.cell] = 1;
        if (current.cell == goal_index) {
            found = true;
            break;
        }
        const GridCoord cc = to_coord(current.cell);

        const auto relax = [&](int dx, int dy, float cost) {
            const GridCoord nc{cc.x + dx, cc.y + dy};
            if (!grid.is_walkable(nc)) return;
            if (dx != 0 && dy != 0) {
                // No corner cutting: both shared orthogonal cells must be open.
                if (!grid.is_walkable({cc.x + dx, cc.y}) || !grid.is_walkable({cc.x, cc.y + dy})) {
                    return;
                }
            }
            const std::size_t ni = to_index(nc);
            if (closed[ni] != 0) return;
            const float tentative = g_score[current.cell] + cost;
            if (tentative < g_score[ni]) {
                g_score[ni] = tentative;
                came_from[ni] = static_cast<int>(current.cell);
                open.push({tentative + heuristic(nc, goal), counter++, ni});
            }
        };

        for (const auto& o : kOrtho) relax(o[0], o[1], 1.0F);
        if (allow_diagonal) {
            for (const auto& d : kDiag) relax(d[0], d[1], kSqrt2);
        }
    }

    if (!found) return path;

    std::size_t cell = goal_index;
    while (true) {
        path.push_back(to_coord(cell));
        if (cell == start_index) break;
        const int prev = came_from[cell];
        if (prev < 0) {
            path.clear();
            return path;
        }
        cell = static_cast<std::size_t>(prev);
    }
    std::reverse(path.begin(), path.end());
    return path;
}

/// Axis-aligned blocker rectangle in world space (x/z plane). Matches the layout
/// of a level collision box's footprint (min_x/min_z/max_x/max_z), so callers can
/// rasterize level collision into a nav grid.
struct NavAABB {
    float min_x {0.0F};
    float min_z {0.0F};
    float max_x {0.0F};
    float max_z {0.0F};
};

/// Build a `width` x `height` NavGrid of `cell_size`, anchored at
/// (origin_x, origin_z) in world space. A cell is blocked when its center lies
/// inside any blocker AABB. Grid Y maps to world Z. Deterministic.
[[nodiscard]] inline NavGrid build_nav_grid(int width, int height, float cell_size,
                                            float origin_x, float origin_z,
                                            const std::vector<NavAABB>& blockers) {
    NavGrid grid(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float cx = origin_x + (static_cast<float>(x) + 0.5F) * cell_size;
            const float cz = origin_z + (static_cast<float>(y) + 0.5F) * cell_size;
            for (const auto& b : blockers) {
                if (cx >= b.min_x && cx <= b.max_x && cz >= b.min_z && cz <= b.max_z) {
                    grid.set_blocked({x, y}, true);
                    break;
                }
            }
        }
    }
    return grid;
}

}  // namespace ahamkara::game::ai
