#pragma once

// Path following for AI navigation: convert a NavGrid cell path into world-space
// waypoints and steer a point along them at a fixed speed. Pure + deterministic
// (no physics); consumes one fixed timestep per advance().

#include "ahamkara/game/ai/nav_grid.h"

#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace ahamkara::game::ai {

struct NavVec2 {
    float x {0.0F};
    float y {0.0F};
};

/// Convert a grid cell path into world-space waypoints at cell centers:
/// world = origin + (cell + 0.5) * cell_size.
[[nodiscard]] inline std::vector<NavVec2> grid_path_to_waypoints(
    const std::vector<GridCoord>& path, float cell_size, NavVec2 origin = {}) {
    std::vector<NavVec2> out;
    out.reserve(path.size());
    for (const auto& c : path) {
        out.push_back({origin.x + (static_cast<float>(c.x) + 0.5F) * cell_size,
                       origin.y + (static_cast<float>(c.y) + 0.5F) * cell_size});
    }
    return out;
}

/// Steers a point along a sequence of world-space waypoints at a fixed speed.
class PathFollower {
public:
    PathFollower() = default;
    explicit PathFollower(std::vector<NavVec2> waypoints) {
        set_waypoints(std::move(waypoints));
    }

    void set_waypoints(std::vector<NavVec2> waypoints) {
        waypoints_ = std::move(waypoints);
        index_ = 0;
    }

    [[nodiscard]] bool finished() const { return index_ >= waypoints_.size(); }
    [[nodiscard]] std::size_t waypoint_index() const { return index_; }
    [[nodiscard]] std::size_t waypoint_count() const { return waypoints_.size(); }

    /// Move `pos` toward the current waypoint by up to speed*dt, advancing to the
    /// next waypoint when within `arrive_radius`. Returns the new position.
    [[nodiscard]] NavVec2 advance(NavVec2 pos, float speed, float dt,
                                  float arrive_radius = 0.05F) {
        float budget = speed * dt;
        if (budget < 0.0F) budget = 0.0F;
        while (budget > 0.0F && index_ < waypoints_.size()) {
            const NavVec2 target = waypoints_[index_];
            const float dx = target.x - pos.x;
            const float dy = target.y - pos.y;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist <= arrive_radius || dist <= 1e-6F) {
                ++index_;
                continue;
            }
            if (budget >= dist) {
                pos = target;
                budget -= dist;
                ++index_;
            } else {
                const float t = budget / dist;
                pos.x += dx * t;
                pos.y += dy * t;
                budget = 0.0F;
            }
        }
        return pos;
    }

private:
    std::vector<NavVec2> waypoints_;
    std::size_t index_ {0};
};

}  // namespace ahamkara::game::ai
