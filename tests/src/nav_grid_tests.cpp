#include "ahamkara/game/ai/nav_grid.h"

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

}  // namespace

int main() {
    if (int r = test_straight_line(); r) return r;
    if (int r = test_start_equals_goal(); r) return r;
    if (int r = test_detour_around_wall(); r) return r;
    if (int r = test_no_path(); r) return r;
    if (int r = test_blocked_endpoints(); r) return r;
    if (int r = test_diagonal_shorter(); r) return r;
    if (int r = test_determinism(); r) return r;
    std::cout << "nav_grid_tests passed\n";
    return 0;
}
