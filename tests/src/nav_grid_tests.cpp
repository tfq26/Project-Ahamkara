#include "ahamkara/game/ai/nav_grid.h"
#include "ahamkara/game/ai/path_follower.h"

#include <iostream>
#include <string>

using ahamkara::game::ai::NavGrid;
using ahamkara::game::ai::GridCoord;
using ahamkara::game::ai::find_path;

namespace {

int fail(const std::string& message) {
    std::cerr << "nav_grid_tests failed: " << message << '\n';
    return 1;
}

bool walkable_path(const NavGrid& grid, const std::vector<GridCoord>& path) {
    for (const auto& c : path) {
        if (grid.is_blocked(c)) return false;
    }
    return true;
}

int test_straight_line() {
    NavGrid grid(5, 5);
    const auto path = find_path(grid, {0, 0}, {3, 0}, false);
    if (path.size() != 4) return fail("straight path should be 4 cells");
    if (path.front() != GridCoord{0, 0} || path.back() != GridCoord{3, 0}) {
        return fail("straight path endpoints wrong");
    }
    return 0;
}

int test_start_equals_goal() {
    NavGrid grid(5, 5);
    const auto path = find_path(grid, {2, 2}, {2, 2}, false);
    if (path.size() != 1 || path[0] != GridCoord{2, 2}) {
        return fail("start==goal should be a single cell");
    }
    return 0;
}

int test_detour_around_wall() {
    NavGrid grid(5, 5);
    for (int y = 0; y < 4; ++y) grid.set_blocked({2, y}, true);  // wall, y=4 open
    const auto path = find_path(grid, {0, 0}, {4, 0}, false);
    if (path.empty()) return fail("should find a detour around the wall");
    if (path.front() != GridCoord{0, 0} || path.back() != GridCoord{4, 0}) {
        return fail("detour endpoints wrong");
    }
    if (path.size() <= 5) return fail("detour should be longer than the blocked straight line");
    if (!walkable_path(grid, path)) return fail("path crosses a blocked cell");
    return 0;
}

int test_no_path() {
    NavGrid grid(5, 5);
    for (int y = 0; y < 5; ++y) grid.set_blocked({2, y}, true);  // full wall
    const auto path = find_path(grid, {0, 0}, {4, 0}, false);
    if (!path.empty()) return fail("walled-off goal should yield no path");
    return 0;
}

int test_blocked_endpoints() {
    NavGrid g1(5, 5);
    g1.set_blocked({0, 0}, true);
    if (!find_path(g1, {0, 0}, {3, 3}, false).empty()) return fail("blocked start should be empty");
    NavGrid g2(5, 5);
    g2.set_blocked({3, 3}, true);
    if (!find_path(g2, {0, 0}, {3, 3}, false).empty()) return fail("blocked goal should be empty");
    return 0;
}

int test_diagonal_shorter() {
    NavGrid grid(5, 5);
    const auto p4 = find_path(grid, {0, 0}, {3, 3}, false);
    const auto p8 = find_path(grid, {0, 0}, {3, 3}, true);
    if (p4.empty() || p8.empty()) return fail("both connectivities should find a path");
    if (p8.size() >= p4.size()) return fail("8-connected path should have fewer cells");
    return 0;
}

int test_determinism() {
    NavGrid grid(8, 8);
    grid.set_blocked({4, 1}, true);
    grid.set_blocked({4, 2}, true);
    grid.set_blocked({4, 3}, true);
    const auto a = find_path(grid, {0, 0}, {7, 7}, true);
    const auto b = find_path(grid, {0, 0}, {7, 7}, true);
    if (a != b) return fail("A* must be deterministic for identical input");
    return 0;
}

int test_grid_path_to_waypoints() {
    using ahamkara::game::ai::grid_path_to_waypoints;
    std::vector<GridCoord> path = {{0, 0}, {1, 0}, {1, 1}};
    const auto wp = grid_path_to_waypoints(path, 2.0F);
    auto near = [](float a, float b) { return std::fabs(a - b) < 1e-4F; };
    if (wp.size() != 3) return fail("waypoint count should match path");
    if (!near(wp[0].x, 1.0F) || !near(wp[0].y, 1.0F)) return fail("cell (0,0) center should be (1,1) at cell_size 2");
    if (!near(wp[2].x, 3.0F) || !near(wp[2].y, 3.0F)) return fail("cell (1,1) center should be (3,3)");
    return 0;
}

int test_path_follower_segment() {
    using ahamkara::game::ai::PathFollower;
    using ahamkara::game::ai::NavVec2;
    auto near = [](float a, float b) { return std::fabs(a - b) < 1e-3F; };
    PathFollower pf({{0.0F, 0.0F}, {5.0F, 0.0F}});
    NavVec2 p{0.0F, 0.0F};
    p = pf.advance(p, 1.0F, 1.0F, 0.01F);                 // move 1 toward (5,0)
    if (!near(p.x, 1.0F) || pf.finished()) return fail("partial move toward waypoint");
    p = pf.advance(p, 100.0F, 1.0F, 0.01F);               // big step reaches end
    if (!near(p.x, 5.0F) || !near(p.y, 0.0F) || !pf.finished()) return fail("should reach final waypoint and finish");
    const NavVec2 q = pf.advance(p, 100.0F, 1.0F);        // advancing when done = no-op
    if (!near(q.x, p.x) || !near(q.y, p.y)) return fail("advance after finish should not move");
    return 0;
}

int test_path_follower_multi() {
    using ahamkara::game::ai::PathFollower;
    using ahamkara::game::ai::NavVec2;
    auto near = [](float a, float b) { return std::fabs(a - b) < 0.05F; };
    PathFollower pf({{0.0F, 0.0F}, {0.0F, 3.0F}, {3.0F, 3.0F}});  // L-shaped
    NavVec2 p{0.0F, 0.0F};
    for (int i = 0; i < 10000 && !pf.finished(); ++i) {
        p = pf.advance(p, 4.0F, 1.0F / 60.0F, 0.01F);
    }
    if (!pf.finished()) return fail("should finish the L path");
    if (!near(p.x, 3.0F) || !near(p.y, 3.0F)) return fail("should end near (3,3)");
    return 0;
}

int test_build_nav_grid_from_blockers() {
    using ahamkara::game::ai::NavAABB;
    using ahamkara::game::ai::build_nav_grid;
    // 10x10 grid, cell 1, origin (0,0). Wall: x in [4,6], z in [0,8] (top rows open).
    std::vector<NavAABB> blockers = {{4.0F, 0.0F, 6.0F, 8.0F}};
    const NavGrid grid = build_nav_grid(10, 10, 1.0F, 0.0F, 0.0F, blockers);
    if (!grid.is_blocked({4, 0}) || !grid.is_blocked({5, 7})) return fail("wall cells should be blocked");
    if (grid.is_blocked({3, 0}) || grid.is_blocked({6, 0})) return fail("cells outside the wall should be open");
    if (grid.is_blocked({4, 9})) return fail("row above the wall (z center 9.5) should be open");
    const auto path = find_path(grid, {0, 0}, {9, 0}, false);
    if (path.empty()) return fail("should detour over the partial wall");
    if (!walkable_path(grid, path)) return fail("path crosses the wall");
    return 0;
}

int test_build_nav_grid_empty() {
    using ahamkara::game::ai::build_nav_grid;
    const NavGrid grid = build_nav_grid(5, 5, 1.0F, 0.0F, 0.0F, {});
    if (grid.is_blocked({2, 2})) return fail("no blockers → all walkable");
    if (find_path(grid, {0, 0}, {4, 4}, false).empty()) return fail("empty grid should be traversable");
    return 0;
}

}  // namespace

int main() {
    if (int r = test_straight_line(); r) return r;
    if (int r = test_start_equals_goal(); r) return r;
    if (int r = test_detour_around_wall(); r) return r;
    if (int r = test_no_path(); r) return r;
    if (int r = test_blocked_endpoints(); r) return r;
    if (int r = test_diagonal_shorter(); r) return r;
    if (int r = test_determinism(); r) return r;
    if (int r = test_grid_path_to_waypoints(); r) return r;
    if (int r = test_path_follower_segment(); r) return r;
    if (int r = test_path_follower_multi(); r) return r;
    if (int r = test_build_nav_grid_from_blockers(); r) return r;
    if (int r = test_build_nav_grid_empty(); r) return r;
    std::cout << "nav_grid_tests passed\n";
    return 0;
}
