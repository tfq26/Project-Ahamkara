#include "ahamkara/game/ai/ai_combatant.h"
#include "ahamkara/game/components.h"

#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <limits>

namespace ahamkara::game::ai {

float angle_diff_deg(float a, float b) {
    float diff = a - b;
    if (diff > 180.0F) diff -= 360.0F;
    else if (diff < -180.0F) diff += 360.0F;
    return diff;
}

bool los_clear_2d(Vec3 from, Vec3 to, const std::vector<ColliderBox>& colliders) {
    float dx = to.x - from.x;
    float dz = to.z - from.z;
    float dist = std::sqrt(dx * dx + dz * dz);
    if (dist < 0.5F) return true;

    for (const auto& c : colliders) {
        if (!c.wall) continue;
        float min_x = std::min(c.min_x, c.max_x);
        float max_x = std::max(c.min_x, c.max_x);
        float min_z = std::min(c.min_z, c.max_z);
        float max_z = std::max(c.min_z, c.max_z);

        float top_y = std::max(c.top_y, c.bottom_y);
        float bottom_y = std::min(c.top_y, c.bottom_y);

        if (top_y < from.y && top_y < to.y) continue;
        if (bottom_y > from.y + 2.0F && bottom_y > to.y + 2.0F) continue;

        float dx_line = to.x - from.x;
        float dz_line = to.z - from.z;
        float len = std::sqrt(dx_line * dx_line + dz_line * dz_line);
        if (len < 0.0001F) continue;

        float nx = -dz_line / len;
        float nz = dx_line / len;

        float proj_cx = nx * ((min_x + max_x) * 0.5F - from.x) +
                        nz * ((min_z + max_z) * 0.5F - from.z);
        float proj_min = nx * (min_x - from.x) + nz * (min_z - from.z);
        float proj_max = nx * (max_x - from.x) + nz * (max_z - from.z);
        float half_extent = std::abs(proj_max - proj_min) * 0.5F;

        if (std::abs(proj_cx) < half_extent + 0.3F) {
            float t = ((min_x + max_x) * 0.5F - from.x) * dx_line +
                      ((min_z + max_z) * 0.5F - from.z) * dz_line;
            t /= (dx_line * dx_line + dz_line * dz_line);
            if (t > 0.01F && t < 0.99F) {
                return false;
            }
        }
    }
    return true;
}

void update_perception(AICombatantComponent& self,
                       const Vec3& self_world_pos,
                       const Vec3& player_pos,
                       float delta_seconds,
                       const std::vector<ColliderBox>& world_colliders) {
    auto& p = self.perception;

    // Distance check
    float dx = player_pos.x - self_world_pos.x;
    float dz = player_pos.z - self_world_pos.z;
    float dist = std::sqrt(dx * dx + dz * dz);

    // Angle check (field of view)
    float target_yaw = std::atan2(dx, dz) * (180.0F / 3.1415926535F);
    float yaw_diff = std::abs(angle_diff_deg(target_yaw, self.yaw));

    bool in_range = dist <= p.detection_range;
    bool in_fov = yaw_diff <= self.cfg.perception_angle_deg * 0.5F;
    bool has_los = false;

    if (in_range && in_fov) {
        Vec3 eye {self_world_pos.x, self_world_pos.y + 1.0F, self_world_pos.z};
        Vec3 target_center {player_pos.x, player_pos.y + 0.9F, player_pos.z};
        has_los = los_clear_2d(eye, target_center, world_colliders);
    }

    if (has_los) {
        p.target_visible = true;
        p.last_seen_timer = 0.0F;
        p.last_known_position = player_pos;
        p.alertness = std::min(1.0F, p.alertness + delta_seconds * 2.0F);
    } else {
        p.target_visible = false;
        p.last_seen_timer += delta_seconds;

        // If we heard something (player firing or being close), increase alertness
        if (dist < p.detection_range * 0.5F && !has_los) {
            p.target_heard = true;
            p.alertness = std::min(1.0F, p.alertness + delta_seconds * 0.5F);
        }

        // Decay alertness if nothing is sensed
        if (!p.target_heard) {
            p.alertness = std::max(0.0F, p.alertness - delta_seconds * 0.2F);
        }
    }

    // Forget after being out of detection for a while
    if (p.last_seen_timer > 8.0F && p.last_seen_timer - delta_seconds <= 8.0F) {
        p.alertness = std::max(0.0F, p.alertness * 0.3F);
    }
}

void update_targeting(AICombatantComponent& self,
                      const Vec3& player_pos,
                      float delta_seconds) {
    (void)delta_seconds;
    // For now the only target is the player.
    self.target_world_pos = player_pos;
    self.target_entity = 0; // player entity ID is always 0 in current setup
}

void tick_behavior(AICombatantComponent& self,
                   float delta_seconds,
                   const Vec3& self_world_pos) {
    self.state_timer += delta_seconds;
    auto& p = self.perception;

    switch (self.behavior) {
        case BehaviorState::Idle:
            if (p.alertness > 0.3F) {
                if (p.target_visible) {
                    self.behavior = BehaviorState::Engage;
                } else {
                    self.behavior = BehaviorState::Investigate;
                    self.investigate_point = {p.last_known_position.x, p.last_known_position.z};
                }
                self.state_timer = 0.0F;
            }
            break;

        case BehaviorState::Investigate:
            // Move toward last known position
            if (p.target_visible) {
                self.behavior = BehaviorState::Engage;
                self.state_timer = 0.0F;
            } else if (self.state_timer > 5.0F || p.alertness < 0.1F) {
                // Give up searching
                self.behavior = BehaviorState::Idle;
                self.state_timer = 0.0F;
            } else if (self.state_timer > 3.0F && !p.target_visible) {
                // After investigating, if still no target, patrol
                self.behavior = BehaviorState::Patrol;
                self.state_timer = 0.0F;
            }
            break;

        case BehaviorState::Engage: {
            // Check distance for ideal range
            float dx = self.target_world_pos.x - self_world_pos.x;
            float dz = self.target_world_pos.z - self_world_pos.z;
            float dist = std::sqrt(dx * dx + dz * dz);

            // Retreat if low health
            if (self.health < self.max_health * 0.25F && dist < 10.0F) {
                self.behavior = BehaviorState::Retreat;
                self.state_timer = 0.0F;
                break;
            }

            // Flank if we've been in combat too long without progress
            if (self.state_timer > 8.0F && !p.target_visible) {
                self.behavior = BehaviorState::Flank;
                self.state_timer = 0.0F;
                break;
            }

            // Too far → move closer; too close → back up
            if (dist > self.cfg.engage_range_max) {
                // Move closer (transition to flank to reposition)
                if (self.state_timer > 3.0F) {
                    self.behavior = BehaviorState::Flank;
                    self.state_timer = 0.0F;
                }
            }

            // Check if we lost the target
            if (!p.target_visible && p.last_seen_timer > 2.0F) {
                self.behavior = BehaviorState::Investigate;
                self.investigate_point = {p.last_known_position.x, p.last_known_position.z};
                self.state_timer = 0.0F;
            }
            break;
        }

        case BehaviorState::Flank:
            // Reposition for a better angle
            if (p.target_visible && self.state_timer > 2.0F) {
                self.behavior = BehaviorState::Engage;
                self.state_timer = 0.0F;
            } else if (self.state_timer > 5.0F) {
                // Return to engage even without seeing target
                self.behavior = BehaviorState::Engage;
                self.state_timer = 0.0F;
            }
            break;

        case BehaviorState::Retreat: {
            // Back off and recover
            if (self.state_timer > 3.0F) {
                if (p.target_visible && self.health > self.max_health * 0.4F) {
                    self.behavior = BehaviorState::Engage;
                } else {
                    self.behavior = BehaviorState::Patrol;
                }
                self.state_timer = 0.0F;
            }
            break;
        }

        case BehaviorState::Patrol:
            // Wander and scan
            if (p.target_visible) {
                self.behavior = BehaviorState::Engage;
                self.state_timer = 0.0F;
            } else if (p.alertness > 0.5F) {
                self.behavior = BehaviorState::Alert;
                self.state_timer = 0.0F;
            }
            break;

        case BehaviorState::Alert:
            if (p.target_visible) {
                self.behavior = BehaviorState::Engage;
                self.state_timer = 0.0F;
            } else if (p.alertness < 0.2F || self.state_timer > 4.0F) {
                self.behavior = BehaviorState::Patrol;
                self.state_timer = 0.0F;
            }
            break;
    }
}

void tick_ai_combatants(entt::registry& registry,
                        float delta_seconds,
                        const Vec3& player_pos,
                        const std::vector<ColliderBox>& world_colliders,
                        bool is_server) {
    if (!is_server) return;

    auto view = registry.view<AICombatantComponent>();
    for (auto entity : view) {
        auto& self = view.get<AICombatantComponent>(entity);
        if (!self.alive) {
            self.respawn_timer -= delta_seconds;
            if (self.respawn_timer <= 0.0F) {
                self.alive = true;
                self.health = self.max_health;
                self.behavior = BehaviorState::Idle;
                self.state_timer = 0.0F;
                self.fire_timer = 0.0F;
                self.burst_remaining = 0;
            }
            continue;
        }

        // Update 2D position from 3D position
        // (self_world_pos is passed separately; we use entity's transform)
        Vec3 self_pos;
        if (registry.all_of<TransformComponent>(entity)) {
            const auto& tc = registry.get<TransformComponent>(entity);
            self_pos = tc.position;
            self.position_2d = {tc.position.x, tc.position.z};
        } else {
            self_pos = {self.position_2d.x, 0.0F, self.position_2d.y};
        }

        // Perception
        update_perception(self, self_pos, player_pos, delta_seconds, world_colliders);
        update_targeting(self, player_pos, delta_seconds);

        // Behavior
        tick_behavior(self, delta_seconds, self_pos);

        // Combat: fire when engaged and have LOS
        if (self.behavior == BehaviorState::Engage || self.behavior == BehaviorState::Flank) {
            if (self.perception.target_visible) {
                self.fire_timer -= delta_seconds;

                if (self.burst_remaining > 0) {
                    self.burst_timer -= delta_seconds;
                    if (self.burst_timer <= 0.0F) {
                        self.burst_timer = 0.12F;  // burst shot interval (120ms)
                        self.burst_remaining--;
                        if (self.burst_remaining <= 0) {
                            self.fire_timer = self.cfg.fire_interval;
                        }
                    }
                } else if (self.fire_timer <= 0.0F) {
                    // Start a new burst
                    self.burst_remaining = self.cfg.burst_count;
                    self.burst_timer = 0.0F;
                }
            }
        }
    }
}

// ============================================================================
// Pathfinding and Movement
// ============================================================================

bool plan_ai_path(AICombatantComponent& self,
                  NavVec2 goal_world,
                  const NavGrid& grid,
                  NavSpace space,
                  bool allow_diagonal) {
    self.move_target = goal_world;
    self.path_waypoints.clear();
    self.path_waypoint_index = 0;

    const GridCoord start = space.world_to_cell(self.position_2d.x, self.position_2d.y);
    const GridCoord goal = space.world_to_cell(goal_world.x, goal_world.y);

    const std::vector<GridCoord> cell_path = find_path(grid, start, goal, allow_diagonal);
    if (cell_path.empty()) {
        return false;
    }

    self.path_waypoints = grid_path_to_waypoints(
        cell_path, space.cell_size,
        NavVec2{space.origin_x, space.origin_z});
    // Snap last waypoint to exact goal
    if (!self.path_waypoints.empty()) {
        self.path_waypoints.back() = goal_world;
    }
    self.path_waypoint_index = 0;
    return true;
}

void advance_along_path(AICombatantComponent& self,
                        float speed, float dt,
                        float arrive_radius) {
    if (self.path_waypoints.empty() || self.path_waypoint_index >= static_cast<int>(self.path_waypoints.size())) {
        self.move_velocity = {0.0F, 0.0F};
        self.move_speed_current = 0.0F;
        return;
    }

    float budget = speed * dt;
    if (budget < 0.0F) budget = 0.0F;

    NavVec2 pos = self.position_2d;

    while (budget > 0.0F && self.path_waypoint_index < static_cast<int>(self.path_waypoints.size())) {
        const NavVec2& target = self.path_waypoints[self.path_waypoint_index];
        const float dx = target.x - pos.x;
        const float dy = target.y - pos.y;
        const float dist = std::sqrt(dx * dx + dy * dy);

        if (dist <= arrive_radius || dist <= 1e-6F) {
            ++self.path_waypoint_index;
            continue;
        }

        if (budget >= dist) {
            pos = target;
            budget -= dist;
            ++self.path_waypoint_index;
        } else {
            const float t = budget / dist;
            pos.x += dx * t;
            pos.y += dy * t;
            budget = 0.0F;
        }
    }

    // If path is exhausted, snap to the final goal
    if (self.path_waypoints.empty() ||
        self.path_waypoint_index >= static_cast<int>(self.path_waypoints.size())) {
        if (!self.path_waypoints.empty()) {
            pos = self.path_waypoints.back();
        }
        self.move_velocity = {0.0F, 0.0F};
        self.move_speed_current = 0.0F;
    } else {
        // Compute velocity from position change
        const float dx_total = pos.x - self.position_2d.x;
        const float dy_total = pos.y - self.position_2d.y;
        const float tick_dt = std::max(dt, 1e-6F);
        self.move_velocity = {dx_total / tick_dt, dy_total / tick_dt};
        self.move_speed_current = std::sqrt(dx_total * dx_total + dy_total * dy_total) / tick_dt;
    }
    self.position_2d = pos;
}

// ============================================================================
// NavGrid building from world colliders
// ============================================================================

NavGridBuildResult build_nav_grid_from_world(
    const std::vector<ColliderBox>& colliders,
    float cell_size,
    float margin) {
    if (colliders.empty()) {
        // Return a minimal grid (1x1) when there are no colliders
        NavGrid grid(1, 1);
        return {std::move(grid), NavSpace{cell_size, 0.0F, 0.0F}};
    }

    // Compute world-space bounding box of all colliders
    float min_x = std::numeric_limits<float>::infinity();
    float max_x = -std::numeric_limits<float>::infinity();
    float min_z = std::numeric_limits<float>::infinity();
    float max_z = -std::numeric_limits<float>::infinity();

    for (const auto& c : colliders) {
        if (!c.wall) continue;
        min_x = std::min(min_x, std::min(c.min_x, c.max_x));
        max_x = std::max(max_x, std::max(c.min_x, c.max_x));
        min_z = std::min(min_z, std::min(c.min_z, c.max_z));
        max_z = std::max(max_z, std::max(c.min_z, c.max_z));
    }

    // If no wall colliders, return a minimal grid
    if (!std::isfinite(min_x)) {
        NavGrid grid(1, 1);
        return {std::move(grid), NavSpace{cell_size, 0.0F, 0.0F}};
    }

    // Expand by margin
    min_x -= margin;
    max_x += margin;
    min_z -= margin;
    max_z += margin;

    const float span_x = max_x - min_x;
    const float span_z = max_z - min_z;
    const int grid_w = std::max(1, static_cast<int>(std::ceil(span_x / cell_size)));
    const int grid_h = std::max(1, static_cast<int>(std::ceil(span_z / cell_size)));

    // Build blocker AABBs from wall colliders
    std::vector<NavAABB> blockers;
    blockers.reserve(colliders.size());
    for (const auto& c : colliders) {
        if (!c.wall) continue;
        blockers.push_back({std::min(c.min_x, c.max_x),
                            std::min(c.min_z, c.max_z),
                            std::max(c.min_x, c.max_x),
                            std::max(c.min_z, c.max_z)});
    }

    NavGrid grid = build_nav_grid(grid_w, grid_h, cell_size, min_x, min_z, blockers);
    NavSpace space{cell_size, min_x, min_z};
    return {std::move(grid), space};
}

// ============================================================================
// Extended tick function: integrates movement with behavior
// ============================================================================

/// Compute movement target and plan path based on the current behavior state.
/// Called from tick_ai_combatants when a NavGrid is available (server only).
void update_ai_movement(AICombatantComponent& self,
                        float delta_seconds,
                        const NavGrid& grid,
                        NavSpace space) {
    const float speed = self.cfg.move_speed;

    switch (self.behavior) {
        case BehaviorState::Idle:
            // Stand still
            self.move_velocity = {0.0F, 0.0F};
            self.move_speed_current = 0.0F;
            self.path_waypoints.clear();
            self.path_waypoint_index = 0;
            break;

        case BehaviorState::Patrol: {
            if (self.patrol_waypoint_count == 0) {
                // No patrol waypoints defined; stand still
                self.move_velocity = {0.0F, 0.0F};
                self.move_speed_current = 0.0F;
                break;
            }

            // Wait at current patrol point if timer is active
            if (self.patrol_wait_timer > 0.0F) {
                self.patrol_wait_timer -= delta_seconds;
                self.move_velocity = {0.0F, 0.0F};
                self.move_speed_current = 0.0F;
                break;
            }

            const NavVec2& target_wp = self.patrol_waypoints[self.patrol_index];
            const float dx = target_wp.x - self.position_2d.x;
            const float dy = target_wp.y - self.position_2d.y;
            const float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < 1.0F) {
                // Reached waypoint; advance to next
                self.patrol_index = (self.patrol_index + 1) % self.patrol_waypoint_count;
                self.patrol_wait_timer = 2.0F;  // Wait 2 seconds at each patrol point
                self.path_waypoints.clear();
                self.path_waypoint_index = 0;
                self.move_velocity = {0.0F, 0.0F};
                self.move_speed_current = 0.0F;
                break;
            }

            // Plan path if needed
            if (self.path_waypoints.empty() || self.path_waypoint_index >= static_cast<int>(self.path_waypoints.size())) {
                (void)plan_ai_path(self, target_wp, grid, space);
            }

            if (!self.path_waypoints.empty()) {
                advance_along_path(self, speed, delta_seconds);
            } else {
                // Fallback: direct movement toward waypoint
                const float inv_dist = 1.0F / std::max(dist, 0.01F);
                const float step = speed * delta_seconds;
                const float move = std::min(step, dist);
                self.position_2d.x += (dx * inv_dist) * move;
                self.position_2d.y += (dy * inv_dist) * move;
                self.move_velocity = {(dx * inv_dist) * speed, (dy * inv_dist) * speed};
                self.move_speed_current = speed;
            }
            break;
        }

        case BehaviorState::Alert:
            // Standing alert — minimal movement, just scanning
            self.move_velocity = {0.0F, 0.0F};
            self.move_speed_current = 0.0F;
            break;

        case BehaviorState::Investigate: {
            const NavVec2 target{self.investigate_point.x, self.investigate_point.y};
            const float dx = target.x - self.position_2d.x;
            const float dy = target.y - self.position_2d.y;
            const float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < 0.5F) {
                // Arrived at investigation point
                self.move_velocity = {0.0F, 0.0F};
                self.move_speed_current = 0.0F;
                self.path_waypoints.clear();
                break;
            }

            // Plan path if needed
            if (self.path_waypoints.empty() || self.path_waypoint_index >= static_cast<int>(self.path_waypoints.size())) {
                (void)plan_ai_path(self, target, grid, space);
            }

            if (!self.path_waypoints.empty()) {
                advance_along_path(self, speed, delta_seconds);
            } else {
                // Fallback: direct movement
                const float inv_dist = 1.0F / std::max(dist, 0.01F);
                const float step = speed * delta_seconds;
                const float move = std::min(step, dist);
                self.position_2d.x += (dx * inv_dist) * move;
                self.position_2d.y += (dy * inv_dist) * move;
                self.move_velocity = {(dx * inv_dist) * speed, (dy * inv_dist) * speed};
                self.move_speed_current = speed;
            }
            break;
        }

        case BehaviorState::Engage: {
            // During engagement, move toward/away from target to maintain ideal range
            const float dx_target = self.target_world_pos.x - self.position_2d.x;
            const float dy_target = self.target_world_pos.z - self.position_2d.y;
            const float dist = std::sqrt(dx_target * dx_target + dy_target * dy_target);

            if (dist < self.cfg.engage_range_min) {
                // Too close — back up
                const float inv_dist = 1.0F / std::max(dist, 0.01F);
                const float step = speed * 0.5F * delta_seconds;
                self.position_2d.x -= (dx_target * inv_dist) * step;
                self.position_2d.y -= (dy_target * inv_dist) * step;
                self.move_velocity = {-(dx_target * inv_dist) * speed * 0.5F,
                                       -(dy_target * inv_dist) * speed * 0.5F};
                self.move_speed_current = speed * 0.5F;
            } else if (dist > self.cfg.engage_range_max) {
                // Too far — move closer
                const float inv_dist = 1.0F / std::max(dist, 0.01F);
                const float step = speed * delta_seconds;
                NavVec2 goal{self.target_world_pos.x, self.target_world_pos.z};

                // Plan path toward target
                if (self.path_waypoints.empty()) {
                    (void)plan_ai_path(self, goal, grid, space);
                }

                if (!self.path_waypoints.empty()) {
                    advance_along_path(self, speed, delta_seconds);
                } else {
                    const float move = std::min(step, dist);
                    self.position_2d.x += (dx_target * inv_dist) * move;
                    self.position_2d.y += (dy_target * inv_dist) * move;
                    self.move_velocity = {(dx_target * inv_dist) * speed,
                                           (dy_target * inv_dist) * speed};
                    self.move_speed_current = speed;
                }
            } else {
                // In ideal range — strafe slightly
                const float dx = self.target_world_pos.x - self.position_2d.x;
                const float dz = self.target_world_pos.z - self.position_2d.y;
                const float d = std::sqrt(dx * dx + dz * dz);
                if (d > 0.1F) {
                    // Perpendicular to target direction (strafe)
                    const float nx = -dz / d;  // Perpendicular direction
                    const float nz = dx / d;
                    const float strafe = std::sin(self.state_timer * 0.5F) * 0.3F;
                    self.position_2d.x += nx * strafe * speed * delta_seconds;
                    self.position_2d.y += nz * strafe * speed * delta_seconds;
                    self.move_velocity = {nx * strafe * speed, nz * strafe * speed};
                    self.move_speed_current = std::abs(strafe) * speed;
                }
                self.path_waypoints.clear();
            }
            break;
        }

        case BehaviorState::Flank: {
            // Move perpendicular to target, then toward flank position
            const float dx = self.target_world_pos.x - self.position_2d.x;
            const float dz = self.target_world_pos.z - self.position_2d.y;
            const float dist = std::sqrt(dx * dx + dz * dz);

            // Compute flank offset (move 90 degrees around the target)
            const float flank_angle = std::atan2(dz, dx) + 1.2F;  // ~69 degrees
            const float flank_dist = 12.0F;
            const NavVec2 flank_pos{
                self.target_world_pos.x + std::cos(flank_angle) * flank_dist,
                self.target_world_pos.z + std::sin(flank_angle) * flank_dist};

            const float fdx = flank_pos.x - self.position_2d.x;
            const float fdy = flank_pos.y - self.position_2d.y;
            const float fdist = std::sqrt(fdx * fdx + fdy * fdy);

            if (fdist < 2.0F) {
                // At flank position
                self.move_velocity = {0.0F, 0.0F};
                self.move_speed_current = 0.0F;
                self.path_waypoints.clear();
                break;
            }

            if (self.path_waypoints.empty()) {
                (void)plan_ai_path(self, flank_pos, grid, space);
            }

            if (!self.path_waypoints.empty()) {
                advance_along_path(self, speed * 1.2F, delta_seconds);  // Flank faster
            } else {
                const float inv_dist = 1.0F / std::max(fdist, 0.01F);
                const float step = speed * 1.2F * delta_seconds;
                const float move = std::min(step, fdist);
                self.position_2d.x += (fdx * inv_dist) * move;
                self.position_2d.y += (fdy * inv_dist) * move;
                self.move_velocity = {(fdx * inv_dist) * speed * 1.2F,
                                       (fdy * inv_dist) * speed * 1.2F};
                self.move_speed_current = speed * 1.2F;
            }
            break;
        }

        case BehaviorState::Retreat: {
            // Move away from target (toward spawn/origin)
            const float dx = self.position_2d.x - self.target_world_pos.x;
            const float dy = self.position_2d.y - self.target_world_pos.z;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > 0.1F) {
                const float inv_dist = 1.0F / dist;
                const float step = speed * 0.7F * delta_seconds;  // Slower when retreating
                self.position_2d.x += (dx * inv_dist) * step;
                self.position_2d.y += (dy * inv_dist) * step;
                self.move_velocity = {(dx * inv_dist) * speed * 0.7F,
                                       (dy * inv_dist) * speed * 0.7F};
                self.move_speed_current = speed * 0.7F;
            } else {
                self.move_velocity = {0.0F, 0.0F};
                self.move_speed_current = 0.0F;
            }
            self.path_waypoints.clear();
            break;
        }
    }
}

void tick_ai_combatants_movement(entt::registry& registry,
                                 float delta_seconds,
                                 const NavGrid& grid,
                                 NavSpace space) {
    auto view = registry.view<AICombatantComponent>();
    for (auto entity : view) {
        auto& self = view.get<AICombatantComponent>(entity);
        if (!self.alive) continue;

        // Update 2D position from 3D position before movement
        if (registry.all_of<TransformComponent>(entity)) {
            const auto& tc = registry.get<TransformComponent>(entity);
            self.position_2d = {tc.position.x, tc.position.z};
        }

        // Apply movement based on behavior state
        update_ai_movement(self, delta_seconds, grid, space);

        // Sync 3D position back from 2D position
        if (registry.all_of<TransformComponent>(entity)) {
            auto& tc = registry.get<TransformComponent>(entity);
            tc.position.x = self.position_2d.x;
            tc.position.z = self.position_2d.y;
        }
    }
}

}  // namespace ahamkara::game::ai
