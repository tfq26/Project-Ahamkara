#include "ahamkara/game/ai/ai_combatant.h"

#include <cmath>
#include <iostream>
#include <string>

using ahamkara::game::ai::CombatArchetype;
using ahamkara::game::ai::archetype_config;
using ahamkara::game::ai::angle_diff_deg;
using ahamkara::game::ai::los_clear_2d;
using ahamkara::game::ai::AICombatantComponent;
using ahamkara::game::ai::BehaviorState;
using ahamkara::game::ai::NavVec2;
using ahamkara::game::ai::NavGrid;
using ahamkara::game::ai::GridCoord;
using ahamkara::game::ai::NavSpace;
using ahamkara::game::ai::NavAABB;
using ahamkara::game::ai::build_nav_grid;
using ahamkara::game::ai::build_nav_grid_from_world;
using ahamkara::game::ai::plan_ai_path;
using ahamkara::game::ai::advance_along_path;
using ahamkara::game::ai::update_ai_movement;
using ahamkara::game::ai::tick_ai_combatants_movement;

#include <entt/entt.hpp>

namespace {

int fail(const std::string& message) {
    std::cerr << "ai_combatant_tests failed: " << message << '\n';
    return 1;
}

auto near = [](float a, float b) { return std::fabs(a - b) < 1e-4F; };
auto near2 = [](float a, float b) { return std::fabs(a - b) < 0.01F; };

// --- Archetype config tests ---

int test_grunt_config() {
    const auto cfg = archetype_config(CombatArchetype::Grunt);
    if (!near(cfg.health, 100.0F)) return fail("grunt health");
    if (!near(cfg.move_speed, 3.0F)) return fail("grunt move_speed");
    if (!near(cfg.perception_range, 50.0F)) return fail("grunt perception_range");
    return 0;
}

int test_sniper_config() {
    const auto cfg = archetype_config(CombatArchetype::Sniper);
    if (!near(cfg.health, 75.0F)) return fail("sniper health");
    if (!near(cfg.engage_range_min, 20.0F)) return fail("sniper engage_range_min");
    if (!near(cfg.perception_range, 80.0F)) return fail("sniper perception_range");
    return 0;
}

int test_rusher_config() {
    const auto cfg = archetype_config(CombatArchetype::Rusher);
    if (!near(cfg.move_speed, 5.5F)) return fail("rusher move_speed");
    if (!near(cfg.engage_range_max, 15.0F)) return fail("rusher engage_range_max");
    return 0;
}

int test_brute_config() {
    const auto cfg = archetype_config(CombatArchetype::Brute);
    if (!near(cfg.health, 200.0F)) return fail("brute health");
    if (!near(cfg.armor, 50.0F)) return fail("brute armor");
    return 0;
}

// --- Angle math tests ---

int test_angle_diff_deg_same() {
    if (!near(angle_diff_deg(45.0F, 45.0F), 0.0F)) return fail("same angle -> 0");
    return 0;
}

int test_angle_diff_deg_positive() {
    if (!near(angle_diff_deg(90.0F, 45.0F), 45.0F)) return fail("90-45 should be 45");
    return 0;
}

int test_angle_diff_deg_negative() {
    if (!near(angle_diff_deg(45.0F, 90.0F), -45.0F)) return fail("45-90 should be -45");
    return 0;
}

int test_angle_diff_deg_wrap() {
    if (!near(angle_diff_deg(350.0F, 10.0F), -20.0F)) return fail("350-10 should wrap to -20");
    return 0;
}

// --- LOS tests ---

int test_los_clear_no_colliders() {
    std::vector<ahamkara::game::ColliderBox> empty;
    if (!los_clear_2d({0.0F,0.0F,0.0F}, {10.0F,0.0F,0.0F}, empty)) return fail("no colliders -> clear");
    return 0;
}

int test_los_blocked_by_wall() {
    std::vector<ahamkara::game::ColliderBox> wall;
    ahamkara::game::ColliderBox cb;
    cb.min_x = 4.0F; cb.min_z = -1.0F;
    cb.max_x = 6.0F; cb.max_z = 1.0F;
    cb.top_y = 1.0F; cb.bottom_y = 0.0F;
    cb.wall = true; cb.jump_through = false; cb.auto_step = true;
    wall.push_back(cb);
    if (los_clear_2d({0.0F,0.0F,0.0F}, {10.0F,0.0F,0.0F}, wall)) return fail("wall should block LOS");
    return 0;
}

// --- Archetype application test ---

int test_apply_archetype() {
    AICombatantComponent comp;
    comp.apply_archetype(CombatArchetype::Brute);
    if (!near(comp.health, 200.0F)) return fail("apply_archetype health");
    if (!near(comp.armor, 50.0F)) return fail("apply_archetype armor");
    if (!near(comp.max_health, 200.0F)) return fail("apply_archetype max_health");
    if (!near(comp.perception.detection_range, 45.0F)) return fail("apply_archetype perception_range");
    return 0;
}

// --- Pathfinding tests (plan_ai_path + advance_along_path) ---

int test_plan_path_straight_line() {
    // Build a simple 5x5 grid with no obstructions
    NavGrid grid(5, 5);
    NavSpace space{1.0F, 0.0F, 0.0F};

    AICombatantComponent comp;
    comp.position_2d = {0.5F, 0.5F};
    comp.apply_archetype(CombatArchetype::Grunt);

    if (!plan_ai_path(comp, {4.5F, 0.5F}, grid, space)) {
        return fail("should find a straight line path");
    }
    if (comp.path_waypoints.empty()) {
        return fail("path waypoints should not be empty");
    }
    // The path should start near the current position and end near the goal
    if (!near2(comp.path_waypoints.front().x, 0.5F) || !near2(comp.path_waypoints.front().y, 0.5F)) {
        return fail("path should start at current position");
    }
    if (!near2(comp.path_waypoints.back().x, 4.5F) || !near2(comp.path_waypoints.back().y, 0.5F)) {
        return fail("path should end at goal position");
    }
    return 0;
}

int test_plan_path_blocked() {
    // Grid with a partial wall — only columns 2-3 at row y=2
    NavGrid grid(5, 5);
    grid.set_blocked({2, 2}, true);
    grid.set_blocked({3, 2}, true);
    NavSpace space{1.0F, 0.0F, 0.0F};

    AICombatantComponent comp;
    comp.position_2d = {0.5F, 0.5F};

    // Goal on the other side of the wall — path should exist (go around via column 1 or 4)
    if (!plan_ai_path(comp, {4.5F, 4.5F}, grid, space)) {
        return fail("should find path around partial wall with 8-connectivity");
    }

    // Now with 4-connectivity — partial wall can also be navigated around
    AICombatantComponent comp2;
    comp2.position_2d = {0.5F, 0.5F};
    if (!plan_ai_path(comp2, {4.5F, 4.5F}, grid, space, false)) {
        return fail("should find path around partial wall with 4-connectivity too");
    }
    return 0;
}

int test_plan_path_completely_blocked() {
    // Full wall from left to right edge at row 2
    NavGrid grid(5, 5);
    for (int x = 0; x < 5; ++x) grid.set_blocked({x, 2}, true);
    NavSpace space{1.0F, 0.0F, 0.0F};

    AICombatantComponent comp;
    comp.position_2d = {0.5F, 0.5F};

    // Goal on the other side of the full wall — no path possible
    if (plan_ai_path(comp, {0.5F, 4.5F}, grid, space)) {
        return fail("full wall should block all paths");
    }
    if (!comp.path_waypoints.empty()) {
        return fail("no path should result in empty waypoints");
    }

    // Same with 4-connectivity
    AICombatantComponent comp2;
    comp2.position_2d = {0.5F, 0.5F};
    if (plan_ai_path(comp2, {0.5F, 4.5F}, grid, space, false)) {
        return fail("full wall should block path with 4-connectivity");
    }
    return 0;
}

int test_plan_path_same_position() {
    NavGrid grid(5, 5);
    NavSpace space{1.0F, 0.0F, 0.0F};

    AICombatantComponent comp;
    comp.position_2d = {2.5F, 2.5F};

    if (!plan_ai_path(comp, {2.5F, 2.5F}, grid, space)) {
        return fail("start == goal should return a valid path");
    }
    if (comp.path_waypoints.empty()) {
        return fail("path should have at least one waypoint");
    }
    return 0;
}

int test_advance_along_path() {
    NavGrid grid(5, 5);
    NavSpace space{1.0F, 0.0F, 0.0F};

    AICombatantComponent comp;
    comp.position_2d = {0.5F, 0.5F};

    // Plan path to the right
    (void)plan_ai_path(comp, {4.5F, 0.5F}, grid, space);
    if (comp.path_waypoints.empty()) return fail("need a path for advance test");

    const float speed = 3.0F;
    const float dt = 1.0F / 60.0F;

    // Simulate several movement steps
    for (int i = 0; i < 600; ++i) {
        advance_along_path(comp, speed, dt, 0.5F);
        if (comp.path_waypoint_index >= static_cast<int>(comp.path_waypoints.size())) {
            break;
        }
    }

    // Should have reached near the goal
    if (!near2(comp.position_2d.x, 4.5F) || !near2(comp.position_2d.y, 0.5F)) {
        return fail("advance should reach goal position");
    }
    return 0;
}

int test_advance_along_path_no_path() {
    AICombatantComponent comp;
    comp.position_2d = {0.0F, 0.0F};

    // No path set — advance should be a no-op
    advance_along_path(comp, 3.0F, 1.0F / 60.0F);

    if (!near2(comp.position_2d.x, 0.0F) || !near2(comp.position_2d.y, 0.0F)) {
        return fail("advancing without a path should not move");
    }
    if (!near2(comp.move_velocity.x, 0.0F) || !near2(comp.move_velocity.y, 0.0F)) {
        return fail("velocity should be zero with no path");
    }
    return 0;
}

// --- NavGrid building from world colliders ---

int test_build_nav_grid_empty_world() {
    std::vector<ahamkara::game::ColliderBox> empty;
    auto result = build_nav_grid_from_world(empty, 1.0F);

    // Should produce a minimal grid
    if (result.grid.width() < 1 || result.grid.height() < 1) {
        return fail("empty world should produce at least 1x1 grid");
    }
    return 0;
}

int test_build_nav_grid_single_wall() {
    std::vector<ahamkara::game::ColliderBox> colliders;
    ahamkara::game::ColliderBox wall;
    wall.min_x = -2.0F; wall.max_x = 2.0F;
    wall.min_z = -0.5F; wall.max_z = 0.5F;
    wall.top_y = 2.0F; wall.bottom_y = 0.0F;
    wall.wall = true;
    colliders.push_back(wall);

    auto result = build_nav_grid_from_world(colliders, 1.0F, 2.0F);

    // Grid should cover the wall plus margin
    if (result.grid.width() < 4 || result.grid.height() < 2) {
        return fail("grid should span the wall area plus margin");
    }

    // Wall cell should be blocked
    // The wall goes from -2 to 2 in x, -0.5 to 0.5 in z
    // With origin at min_x=margin=-2-2=-4, min_z=margin=-0.5-2=-2.5
    // Cell (0,0) center at (-3.5, -2.0) — outside wall
    // Wall cells: x from -2 to 2 → in grid at x≈1 to 5, z at -0.5 to 0.5 → in grid at y≈2 to 3
    bool found_blocked = false;
    for (int y = 0; y < result.grid.height(); ++y) {
        for (int x = 0; x < result.grid.width(); ++x) {
            if (result.grid.is_blocked({x, y})) {
                found_blocked = true;
                break;
            }
        }
        if (found_blocked) break;
    }
    if (!found_blocked) {
        return fail("wall should produce blocked grid cells");
    }
    return 0;
}

int test_build_nav_grid_non_wall_ignored() {
    std::vector<ahamkara::game::ColliderBox> colliders;
    ahamkara::game::ColliderBox floor;
    floor.min_x = -10.0F; floor.max_x = 10.0F;
    floor.min_z = -10.0F; floor.max_z = 10.0F;
    floor.top_y = 0.3F; floor.bottom_y = 0.0F;
    floor.wall = false;  // not a wall — should not affect nav grid
    colliders.push_back(floor);

    auto result = build_nav_grid_from_world(colliders, 1.0F, 0.0F);

    // No wall colliders → should produce minimal grid
    if (result.grid.width() > 2 || result.grid.height() > 2) {
        return fail("non-wall colliders should not produce a large grid");
    }
    return 0;
}

// --- update_ai_movement tests ---

int test_movement_idle() {
    NavGrid grid(3, 3);
    NavSpace space{1.0F, 0.0F, 0.0F};

    AICombatantComponent comp;
    comp.position_2d = {1.5F, 1.5F};
    comp.behavior = BehaviorState::Idle;
    comp.cfg.move_speed = 3.0F;

    update_ai_movement(comp, 1.0F / 60.0F, grid, space);

    if (!near2(comp.move_velocity.x, 0.0F) || !near2(comp.move_velocity.y, 0.0F)) {
        return fail("idle should have zero velocity");
    }
    if (!near2(comp.position_2d.x, 1.5F) || !near2(comp.position_2d.y, 1.5F)) {
        return fail("idle should not move");
    }
    return 0;
}

int test_movement_investigate() {
    // Build a simple grid
    NavGrid grid(5, 5);
    NavSpace space{1.0F, 0.0F, 0.0F};

    AICombatantComponent comp;
    comp.position_2d = {0.5F, 0.5F};
    comp.behavior = BehaviorState::Investigate;
    comp.investigate_point = {3.5F, 3.5F};
    comp.cfg.move_speed = 3.0F;

    update_ai_movement(comp, 1.0F / 60.0F, grid, space);

    // Should have a path and velocity toward the investigation point
    if (comp.path_waypoints.empty()) {
        return fail("investigate should plan a path");
    }
    if (near2(comp.move_velocity.x, 0.0F) && near2(comp.move_velocity.y, 0.0F)) {
        return fail("investigate should have non-zero velocity");
    }
    // Position should move toward the target
    if (comp.position_2d.x <= 0.5F) {
        return fail("investigate should move toward target");
    }
    return 0;
}

int test_movement_patrol() {
    NavGrid grid(5, 5);
    NavSpace space{1.0F, 0.0F, 0.0F};

    AICombatantComponent comp;
    comp.position_2d = {0.5F, 0.5F};
    comp.behavior = BehaviorState::Patrol;
    comp.cfg.move_speed = 3.0F;

    // Set two patrol waypoints
    NavVec2 waypoints[] = {{4.5F, 0.5F}, {4.5F, 4.5F}};
    comp.patrol_waypoint_count = 2;
    comp.patrol_waypoints[0] = waypoints[0];
    comp.patrol_waypoints[1] = waypoints[1];
    comp.patrol_index = 0;

    update_ai_movement(comp, 1.0F / 60.0F, grid, space);

    // Should be moving toward the first waypoint
    if (comp.path_waypoints.empty()) {
        return fail("patrol should plan a path to waypoint");
    }
    if (near2(comp.move_velocity.x, 0.0F) && near2(comp.move_velocity.y, 0.0F)) {
        return fail("patrol should have non-zero velocity");
    }
    return 0;
}

int test_movement_retreat() {
    NavGrid grid(3, 3);
    NavSpace space{1.0F, 0.0F, 0.0F};

    AICombatantComponent comp;
    comp.position_2d = {1.5F, 1.5F};
    comp.behavior = BehaviorState::Retreat;
    comp.cfg.move_speed = 3.0F;
    comp.target_world_pos = {0.5F, 0.0F, 0.5F};  // Target at origin

    update_ai_movement(comp, 1.0F / 60.0F, grid, space);

    // Retreat should move away from target (toward positive x/z)
    if (comp.position_2d.x <= 1.5F || comp.position_2d.y <= 1.5F) {
        return fail("retreat should move away from target");
    }
    return 0;
}

int test_tick_ai_combatants_movement() {
    entt::registry registry;
    NavGrid grid(5, 5);
    NavSpace space{1.0F, 0.0F, 0.0F};

    // Create an AI combatant entity
    auto entity = registry.create();
    AICombatantComponent comp;
    comp.position_2d = {0.5F, 0.5F};
    comp.behavior = BehaviorState::Investigate;
    comp.investigate_point = {4.5F, 4.5F};
    comp.cfg.move_speed = 3.0F;
    comp.apply_archetype(CombatArchetype::Grunt);
    registry.emplace<AICombatantComponent>(entity, std::move(comp));

    // First, plan the path directly to verify
    auto& check = registry.get<AICombatantComponent>(entity);
    bool path_ok = plan_ai_path(check, {4.5F, 4.5F}, grid, space);
    if (!path_ok) return fail("path planning should succeed in open grid");
    if (check.path_waypoints.empty()) return fail("path should have waypoints");
    // Clear and let the tick function re-plan
    check.path_waypoints.clear();
    check.path_waypoint_index = 0;

    // Tick movement for enough frames to reach near the target
    const float dt = 1.0F / 60.0F;
    for (int i = 0; i < 300; ++i) {
        tick_ai_combatants_movement(registry, dt, grid, space);
    }

    // Should have moved toward the investigation point
    const auto& final_comp = registry.get<AICombatantComponent>(entity);
    if (final_comp.position_2d.x < 4.0F) {
        return fail("combatant should have moved significantly toward investigate target");
    }
    return 0;
}

}  // namespace

int main() {
    if (int r = test_grunt_config(); r) return r;
    if (int r = test_sniper_config(); r) return r;
    if (int r = test_rusher_config(); r) return r;
    if (int r = test_brute_config(); r) return r;
    if (int r = test_angle_diff_deg_same(); r) return r;
    if (int r = test_angle_diff_deg_positive(); r) return r;
    if (int r = test_angle_diff_deg_negative(); r) return r;
    if (int r = test_angle_diff_deg_wrap(); r) return r;
    if (int r = test_los_clear_no_colliders(); r) return r;
    if (int r = test_los_blocked_by_wall(); r) return r;
    if (int r = test_apply_archetype(); r) return r;
    if (int r = test_plan_path_straight_line(); r) return r;
    if (int r = test_plan_path_blocked(); r) return r;
    if (int r = test_plan_path_completely_blocked(); r) return r;
    if (int r = test_plan_path_same_position(); r) return r;
    if (int r = test_advance_along_path(); r) return r;
    if (int r = test_advance_along_path_no_path(); r) return r;
    if (int r = test_build_nav_grid_empty_world(); r) return r;
    if (int r = test_build_nav_grid_single_wall(); r) return r;
    if (int r = test_build_nav_grid_non_wall_ignored(); r) return r;
    if (int r = test_movement_idle(); r) return r;
    if (int r = test_movement_investigate(); r) return r;
    if (int r = test_movement_patrol(); r) return r;
    if (int r = test_movement_retreat(); r) return r;
    if (int r = test_tick_ai_combatants_movement(); r) return r;
    std::cout << "ai_combatant_tests passed\n";
    return 0;
}
