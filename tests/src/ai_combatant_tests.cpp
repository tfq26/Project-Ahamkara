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

namespace {

int fail(const std::string& message) {
    std::cerr << "ai_combatant_tests failed: " << message << '\n';
    return 1;
}

auto near = [](float a, float b) { return std::fabs(a - b) < 1e-4F; };

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
    std::cout << "ai_combatant_tests passed\n";
    return 0;
}
