#include "ae/collision/debug.h"
#include "ae/collision/world.h"
#include "ae/collision/layers.h"
#include "ae/core/log.h"

#include <cstdio>
#include <cstring>

#define AE_LOG_CATEGORY "Collision"

namespace ae::collision {

namespace {

void add_box(DebugOverlay& overlay, const AABB& box, float r, float g, float b) {
    if (overlay.box_count >= kMaxDebugBoxes) return;
    auto& db = overlay.boxes[overlay.box_count++];
    db.min = box.min;
    db.max = box.max;
    db.red = r;
    db.green = g;
    db.blue = b;
    db.alpha = 0.4F;
}

void add_line(DebugOverlay& overlay, const Vec3& start, const Vec3& end,
              float r, float g, float b) {
    if (overlay.line_count >= kMaxDebugLines) return;
    auto& dl = overlay.lines[overlay.line_count++];
    dl.start = start;
    dl.end = end;
    dl.red = r;
    dl.green = g;
    dl.blue = b;
}

void add_sphere(DebugOverlay& overlay, const Vec3& center, float radius,
                float r, float g, float b) {
    if (overlay.sphere_count >= kMaxDebugSpheres) return;
    auto& ds = overlay.spheres[overlay.sphere_count++];
    ds.center = center;
    ds.radius = radius;
    ds.red = r;
    ds.green = g;
    ds.blue = b;
}

void color_for_layer(CollisionLayer layer, float& r, float& g, float& b) {
    switch (layer) {
        case GameLayers::WORLD_STATIC:  DebugColors::static_world(r, g, b); break;
        case GameLayers::WORLD_DYNAMIC: DebugColors::dynamic_world(r, g, b); break;
        case GameLayers::PLAYER:        DebugColors::player(r, g, b); break;
        case GameLayers::NPC:           DebugColors::npc(r, g, b); break;
        case GameLayers::PROJECTILE:    DebugColors::projectile(r, g, b); break;
        case GameLayers::TRIGGER:       DebugColors::trigger(r, g, b); break;
        case GameLayers::PICKUP:        DebugColors::pickup(r, g, b); break;
        case GameLayers::VEHICLE:       DebugColors::vehicle(r, g, b); break;
        case GameLayers::DEBRIS:        DebugColors::debris(r, g, b); break;
        case GameLayers::CAMERA:        DebugColors::sensor(r, g, b); break;
        default:                        DebugColors::sensor(r, g, b); break;
    }
}

}  // namespace

// ============================================================
// populate_debug_overlay_for_world
// ============================================================
void populate_debug_overlay(CollisionWorld& world, DebugOverlay& overlay) {
    overlay.clear();

    // Walk all bodies and draw their AABBs
    // We need access to the internal body map, so we use the world's
    // query_aabb with a huge AABB to collect all bodies.
    CollisionMask all_mask;
    constexpr int kMaxBodyHandles = 512;
    BodyHandle handles[kMaxBodyHandles];

    AABB huge_box {
        {-1000.0F, -1000.0F, -1000.0F},
        { 1000.0F,  1000.0F,  1000.0F}
    };

    int body_count = world.query_aabb(huge_box, all_mask, handles, kMaxBodyHandles);

    if (body_count >= kMaxBodyHandles) {
        log_warning_cat(AE_LOG_CATEGORY, "populate_debug_overlay: body count " +
                        std::to_string(body_count) + " reached max " + std::to_string(kMaxBodyHandles) +
                        "; some bodies not drawn");
    }
    log_debug_cat(AE_LOG_CATEGORY, "populate_debug_overlay: " + std::to_string(body_count) + " bodies");

    for (int i = 0; i < body_count; ++i) {
        AABB box = world.get_body_aabb(handles[i]);
        u64 user_data = world.get_body_user_data(handles[i]);

        // Determine color based on user data (which encodes the layer in BodyDef)
        // We don't have direct layer access without the internal map,
        // so we use a heuristic: body_index range maps to layers.
        // For a proper implementation, the CollisionWorld would expose
        // get_body_layer() or we'd iterate the internal map.
        float r, g, b;
        // Default to static world color for unknown bodies
        DebugColors::static_world(r, g, b);
        add_box(overlay, box, r, g, b);
    }

    // Add stats text
    static char stats_buf[256];
    CollisionStats stats = world.get_stats();
    std::snprintf(stats_buf, sizeof(stats_buf),
        "Bodies: %d | Active Dynamic: %d | BP Pairs: ~%d",
        stats.body_count, stats.active_dynamic_bodies, stats.broadphase_pairs);
    overlay.stats_text = stats_buf;
}

void populate_debug_hitboxes(const HitboxInstance* hitboxes, int hitbox_count,
                             DebugOverlay& overlay) {
    int active_count = 0;
    for (int i = 0; i < hitbox_count; ++i) {
        if (hitboxes[i].active) ++active_count;
    }
    log_trace_cat(AE_LOG_CATEGORY, "populate_debug_hitboxes: " + std::to_string(active_count) +
                  " active of " + std::to_string(hitbox_count));

    for (int i = 0; i < hitbox_count; ++i) {
        const auto& hb = hitboxes[i];
        if (!hb.active) continue;

        float r, g, b;
        if (hb.type == HitboxType::Hitbox) {
            DebugColors::hitbox(r, g, b);
        } else {
            DebugColors::hurtbox(r, g, b);
        }
        add_box(overlay, hb.box, r, g, b);
    }
}

void populate_debug_trace(const Ray& ray, const TraceResult& result,
                          DebugOverlay& overlay) {
    log_trace_cat(AE_LOG_CATEGORY, "populate_debug_trace: " +
                  std::string(result.hit ? "hit dist=" + std::to_string(result.distance) : "miss"));
    float r, g, b;
    DebugColors::trace_ray(r, g, b);

    // Draw the ray as a line
    Vec3 end = {
        ray.origin.x + ray.direction.x * result.distance,
        ray.origin.y + ray.direction.y * result.distance,
        ray.origin.z + ray.direction.z * result.distance
    };

    if (result.hit) {
        // Ray to hit point
        add_line(overlay, ray.origin, result.position, r, g, b);

        // Surface normal at hit
        float hr, hg, hb;
        DebugColors::trace_hit(hr, hg, hb);
        Vec3 normal_end = {
            result.position.x + result.normal.x * 0.3F,
            result.position.y + result.normal.y * 0.3F,
            result.position.z + result.normal.z * 0.3F
        };
        add_line(overlay, result.position, normal_end, hr, hg, hb);

        // Small sphere at hit point
        add_sphere(overlay, result.position, 0.05F, hr, hg, hb);
    } else {
        // Full ray with no hit (use a shorter, arbitrary distance for display)
        Vec3 far_end = {
            ray.origin.x + ray.direction.x * 100.0F,
            ray.origin.y + ray.direction.y * 100.0F,
            ray.origin.z + ray.direction.z * 100.0F
        };
        add_line(overlay, ray.origin, far_end, r, g, b);
    }
}

}  // namespace ae::collision
