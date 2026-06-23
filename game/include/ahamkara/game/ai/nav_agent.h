#pragma once

// A navigation agent: plans an A* path to a world-space goal on a NavGrid and
// follows it on the fixed timestep. Pure + deterministic; the owner (e.g. World)
// holds the real entity and calls update() with the fixed dt. The 2D plane is
// world (x, z); NavVec2.x = world x and NavVec2.y = world z.

#include "ahamkara/game/ai/nav_grid.h"
#include "ahamkara/game/ai/path_follower.h"

#include <cmath>
#include <utility>
#include <vector>

namespace ahamkara::game::ai {

/// Maps between world (x, z) coordinates and NavGrid cells.
struct NavSpace {
    float cell_size {1.0F};
    float origin_x {0.0F};
    float origin_z {0.0F};

    [[nodiscard]] GridCoord world_to_cell(float wx, float wz) const {
        return GridCoord{static_cast<int>(std::floor((wx - origin_x) / cell_size)),
                         static_cast<int>(std::floor((wz - origin_z) / cell_size))};
    }
    [[nodiscard]] NavVec2 cell_center(GridCoord c) const {
        return NavVec2{origin_x + (static_cast<float>(c.x) + 0.5F) * cell_size,
                       origin_z + (static_cast<float>(c.y) + 0.5F) * cell_size};
    }
};

class NavAgent {
public:
    NavAgent(const NavGrid& grid, NavSpace space, bool allow_diagonal = false)
        : grid_(&grid), space_(space), allow_diagonal_(allow_diagonal) {}

    void set_position(NavVec2 p) { pos_ = p; }
    [[nodiscard]] NavVec2 position() const { return pos_; }

    /// Plan a path from the current position to `goal_world`. Returns true if a
    /// walkable path was found; on failure the agent is left without a path.
    bool set_goal(NavVec2 goal_world) {
        const GridCoord start = space_.world_to_cell(pos_.x, pos_.y);
        const GridCoord goal = space_.world_to_cell(goal_world.x, goal_world.y);
        path_ = find_path(*grid_, start, goal, allow_diagonal_);
        if (path_.empty()) {
            follower_.set_waypoints({});
            return false;
        }
        std::vector<NavVec2> waypoints = grid_path_to_waypoints(
            path_, space_.cell_size, NavVec2{space_.origin_x, space_.origin_z});
        // Land on the exact goal rather than the final cell's center.
        waypoints.back() = goal_world;
        follower_.set_waypoints(std::move(waypoints));
        return true;
    }

    /// Advance along the planned path by up to speed*dt this fixed step.
    void update(float speed, float dt, float arrive_radius = 0.05F) {
        pos_ = follower_.advance(pos_, speed, dt, arrive_radius);
    }

    [[nodiscard]] bool has_path() const { return !path_.empty(); }
    [[nodiscard]] bool at_goal() const { return follower_.finished(); }
    [[nodiscard]] const std::vector<GridCoord>& path_cells() const { return path_; }

private:
    const NavGrid* grid_;
    NavSpace space_;
    bool allow_diagonal_;
    NavVec2 pos_ {};
    std::vector<GridCoord> path_;
    PathFollower follower_;
};

}  // namespace ahamkara::game::ai
