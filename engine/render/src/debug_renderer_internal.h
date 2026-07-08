#pragma once

// Internal declarations shared across debug renderer implementation files.
// Not part of the public API — consumed only by debug_renderer*.cpp.

#include "ae/render/debug_renderer.h"
#include "ae/render/font_atlas.h"
#include "ae/render/frustum.h"
#include "ae/render/gl_platform.h"
#include "ae/render/render_backend.h"
#include "ae/render/skeletal_animation.h"

#include <array>
#include <cstdint>
#include <string>

namespace ae::render {

// --- Shared font atlas (defined in debug_renderer.cpp) ----------------------
FontAtlas& shared_ui_font_atlas();

// --- Shared constants ------------------------------------------------------
extern const float kPi;

// --- Simple 3D / 2D primitives (defined in debug_renderer.cpp) --------------
struct LocalMat4 {
    std::array<float, 16> values {};
};

Vec3 subtract(Vec3 lhs, Vec3 rhs);
Vec3 cross(Vec3 lhs, Vec3 rhs);
float dot(Vec3 lhs, Vec3 rhs);
Vec3 normalize(Vec3 value);
LocalMat4 perspective(float vertical_fov_radians, float aspect_ratio, float near_plane, float far_plane);
LocalMat4 look_at(Vec3 eye, Vec3 target, Vec3 world_up);

void set_color(float red, float green, float blue);
void draw_line(Vec3 from, Vec3 to);
void draw_ground_grid(int half_extent, float spacing, float brightness = 1.0F);
void draw_axes();
void draw_box(Vec3 min, Vec3 max);
void draw_player_marker(Vec3 position, float height, float yaw);
void draw_screen_quad(float x, float y, float width, float height);
void draw_screen_line(float x1, float y1, float x2, float y2);
void draw_screen_line_loop(const float* points, int point_count);
void draw_screen_triangle_fan(const float* points, int point_count);
void draw_screen_triangle_strip(const float* points, int point_count);
void draw_screen_triangles(const float* points, int point_count);
void draw_screen_points(const float* points, int point_count);

// --- Bitmap font glyph lookup (defined in debug_renderer.cpp) ---------------
const std::array<std::uint8_t, 7>* glyph_for_char(char character);

// --- Text rendering (defined in debug_renderer.cpp) -------------------------
enum class UiTextStyle {
    Header,
    Section,
    Body,
    Accent,
    Muted,
    Inverted
};

void draw_text(float x, float y, float scale, const std::string& text);
void draw_ui_text(float x, float y, float scale, const std::string& text, UiTextStyle style);

// --- UI primitives (defined in debug_renderer.cpp) --------------------------
void draw_panel(float x, float y, float width, float height, float r, float g, float b, float a);
void draw_panel_outline(float x, float y, float width, float height, float r, float g, float b, float a);
void draw_circle(float center_x, float center_y, float radius, float red, float green, float blue, float alpha);
void draw_button_chip(float x, float y, float width, float height, const char* label, float accent_r, float accent_g, float accent_b);
void draw_xbox_button_legend(unsigned int controller_buttons, float origin_x, float origin_y);

// --- Formatting helpers (defined in debug_renderer.cpp) ---------------------
std::string format_overlay_line(const char* label, double value, const char* suffix = "");
std::string format_integer_overlay_line(const char* label, double value, const char* suffix = "");
std::string format_memory_line(const char* label, double used_mb, double total_mb);

// --- Metrics overlay (debug_renderer_metrics.cpp) ---------------------------
void draw_metrics_overlay(const DebugScene& scene, int width, int height,
                          const std::array<double, 200>& frame_time_history,
                          int sparkline_count);
void draw_gpu_profiler_overlay(const DebugScene& scene, int width, int height);

// --- HUD / debug overlays (debug_renderer_hud.cpp) --------------------------
void draw_crosshair_overlay(const DebugScene& scene, int width, int height);
void draw_hud(const DebugScene& scene, int width, int height, float hud_brightness);
void draw_damage_flash_overlay(const DebugScene& scene, int width, int height);
void draw_viewmodel_placeholder(const DebugScene& scene, int width, int height);
void draw_menu_overlay(const DebugScene& scene, int width, int height);
void draw_scene_overlay(const DebugScene& scene, int width, int height);

// --- Particle / decal effects (debug_renderer_effects.cpp) ------------------
void draw_particles(RenderBackend& backend, BufferHandle& particle_vbo,
                    BufferHandle& particle_color_vbo, RenderStats& stats,
                    const DebugScene& scene, const LocalMat4& view,
                    const Frustum& frustum);
void draw_decals(RenderBackend& backend, BufferHandle& decal_vbo,
                 BufferHandle& decal_color_vbo, RenderStats& stats,
                 const DebugScene& scene, const Frustum& frustum);

// --- GPU model drawing (debug_renderer_gpu.cpp) -----------------------------
void draw_gpu_model(RenderBackend& backend, const GpuModel& model,
                    ShaderHandle shader, int color_loc,
                    int use_skinning_loc, int joints_mat_loc,
                    float r, float g, float b, float a,
                    bool use_mesh_colors,
                    const ae::render::Mat4* joint_matrices,
                    int joint_count);

}  // namespace ae::render
