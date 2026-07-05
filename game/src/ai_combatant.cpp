#include "ahamkara/game/ai/ai_combatant.h"
#include "ahamkara/game/components.h"

#include <cmath>
#include <algorithm>
#include <cstdlib>

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

}  // namespace ahamkara::game::ai
