#pragma once

/// SAFE for headless/server — no render dependencies.
/// Extracts plain data; debug line generation produces abstract line data
/// that any renderer can consume.

#include "ahamkara/game/movement.h"
#include "ahamkara/game/net_types.h"

namespace ahamkara::game::adapters {

// ---------------------------------------------------------------------------
// Flat, trivially-copyable snapshot of movement debug state.
// Safe to serialize or copy across module boundaries.
// ---------------------------------------------------------------------------
struct MovementDebugSnapshot {
    Vec3  velocity_vector   {};
    Vec3  wish_direction    {};
    Vec3  ground_normal     {};
    float jump_buffer_pct   {0.0f};
    float coyote_pct        {0.0f};
    float slide_pct         {0.0f};
    bool  on_ground         {false};
    bool  on_ladder         {false};
    int   ground_material   {0}; // SurfaceMaterial cast to int
};

/// Extract a flat snapshot from the full MovementDebugState.
/// Safe for headless/server — pure data copy, no rendering calls.
[[nodiscard]] inline MovementDebugSnapshot extract_movement_debug_snapshot(
    const MovementDebugState& state)
{
    MovementDebugSnapshot snap;
    snap.velocity_vector  = state.velocity_vector;
    snap.wish_direction   = state.wish_direction;
    snap.ground_normal    = state.ground_normal;
    snap.jump_buffer_pct  = state.jump_buffer_pct;
    snap.coyote_pct       = state.coyote_pct;
    snap.slide_pct        = state.slide_pct;
    snap.on_ground        = state.on_ground;
    snap.on_ladder        = state.on_ladder;
    snap.ground_material  = static_cast<int>(state.ground_material);
    return snap;
}

// ---------------------------------------------------------------------------
// Abstract debug line — start, end, and RGB colour.
// No renderer dependency; the consumer decides how to draw it.
// ---------------------------------------------------------------------------
struct DebugLine {
    Vec3 start {};
    Vec3 end   {};
    float r {1.0f};
    float g {1.0f};
    float b {1.0f};
};

/// Fixed-capacity container for debug lines.
struct MovementDebugLines {
    static constexpr int kMaxLines = 32;
    DebugLine lines[kMaxLines] {};
    int count {0};
};

/// Generate coloured debug lines from a movement snapshot.
///
/// Produces:
///   - Velocity vector (cyan) from origin
///   - Wish direction (yellow) from origin
///   - Ground normal (green) from origin
///   - Jump buffer bar (purple → white with fullness)
///   - Coyote indicator (orange when active)
///   - On-ground / on-ladder status lines
///
/// Lines are appended into out_lines; caller must reset count before calling
/// if reusing the same buffer.
inline void make_movement_debug_lines(
    const MovementDebugSnapshot& snap,
    const Vec3& origin,
    MovementDebugLines& out_lines)
{
    // Velocity — cyan
    if (out_lines.count < MovementDebugLines::kMaxLines) {
        auto& l = out_lines.lines[out_lines.count++];
        l.start = origin;
        l.end   = Vec3{origin.x + snap.velocity_vector.x,
                       origin.y + snap.velocity_vector.y,
                       origin.z + snap.velocity_vector.z};
        l.r = 0.0f; l.g = 1.0f; l.b = 1.0f;
    }

    // Wish direction — yellow
    if (out_lines.count < MovementDebugLines::kMaxLines) {
        auto& l = out_lines.lines[out_lines.count++];
        l.start = origin;
        l.end   = Vec3{origin.x + snap.wish_direction.x * 2.0f,
                       origin.y + snap.wish_direction.y * 2.0f,
                       origin.z + snap.wish_direction.z * 2.0f};
        l.r = 1.0f; l.g = 1.0f; l.b = 0.0f;
    }

    // Ground normal — green
    if (out_lines.count < MovementDebugLines::kMaxLines) {
        auto& l = out_lines.lines[out_lines.count++];
        l.start = origin;
        l.end   = Vec3{origin.x + snap.ground_normal.x * 1.5f,
                       origin.y + snap.ground_normal.y * 1.5f,
                       origin.z + snap.ground_normal.z * 1.5f};
        l.r = 0.0f; l.g = 1.0f; l.b = 0.0f;
    }

    // Jump buffer bar — purple → white as it fills
    if (snap.jump_buffer_pct > 0.0f && out_lines.count < MovementDebugLines::kMaxLines) {
        auto& l = out_lines.lines[out_lines.count++];
        l.start = Vec3{origin.x - 0.5f, origin.y + 0.1f, origin.z};
        l.end   = Vec3{origin.x - 0.5f + snap.jump_buffer_pct,
                       origin.y + 0.1f, origin.z};
        l.r = 0.5f + snap.jump_buffer_pct * 0.5f;
        l.g = 0.0f + snap.jump_buffer_pct * 1.0f;
        l.b = 1.0f;
    }

    // Coyote indicator — orange when active
    if (snap.coyote_pct > 0.0f && out_lines.count < MovementDebugLines::kMaxLines) {
        auto& l = out_lines.lines[out_lines.count++];
        l.start = Vec3{origin.x - 0.5f, origin.y - 0.1f, origin.z};
        l.end   = Vec3{origin.x - 0.5f + snap.coyote_pct,
                       origin.y - 0.1f, origin.z};
        l.r = 1.0f; l.g = 0.5f; l.b = 0.0f;
    }

    // Slide indicator — red bar when sliding
    if (snap.slide_pct > 0.0f && out_lines.count < MovementDebugLines::kMaxLines) {
        auto& l = out_lines.lines[out_lines.count++];
        l.start = Vec3{origin.x - 0.5f, origin.y - 0.3f, origin.z};
        l.end   = Vec3{origin.x - 0.5f + snap.slide_pct,
                       origin.y - 0.3f, origin.z};
        l.r = 1.0f; l.g = 0.2f; l.b = 0.2f;
    }

    // On-ladder indicator — blue cross above origin
    if (snap.on_ladder && out_lines.count < MovementDebugLines::kMaxLines) {
        auto& l = out_lines.lines[out_lines.count++];
        l.start = Vec3{origin.x - 0.2f, origin.y + 1.0f, origin.z};
        l.end   = Vec3{origin.x + 0.2f, origin.y + 1.0f, origin.z};
        l.r = 0.3f; l.g = 0.5f; l.b = 1.0f;
    }
}

}  // namespace ahamkara::game::adapters
