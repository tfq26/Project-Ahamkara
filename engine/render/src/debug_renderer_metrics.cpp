// SPDX-License-Identifier: MIT
// Profiler / metrics overlay drawing for the debug renderer.

#include "debug_renderer_internal.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include "gl_compat.h"

namespace ae::render {

void draw_metrics_overlay(const DebugScene& scene, int width, int height,
                          const std::array<double, 200>& frame_time_history,
                          int sparkline_count) {
    // --- Color threshold for frame time ---
    const double ft = scene.frame_time_ms;
    float ft_r = 0.92F, ft_g = 0.96F, ft_b = 0.99F;  // default neutral
    if (ft < 8.3)  { ft_r = 0.25F; ft_g = 0.88F; ft_b = 0.35F; }  // green  (<120fps safe)
    else if (ft < 16.7) { ft_r = 0.95F; ft_g = 0.72F; ft_b = 0.15F; } // yellow (>60fps safe)
    else            { ft_r = 0.95F; ft_g = 0.25F; ft_b = 0.20F; }  // red    (<60fps)

    char fps_buf[32];
    std::snprintf(fps_buf, sizeof(fps_buf), "%.0f", scene.fps);
    char frame_buf[32];
    std::snprintf(frame_buf, sizeof(frame_buf), "%.1f MS", ft);
    char cpu_buf[32];
    std::snprintf(cpu_buf, sizeof(cpu_buf), "%.1f%%", static_cast<double>(scene.process_cpu_percent));
    char sys_cpu_buf[32];
    std::snprintf(sys_cpu_buf, sizeof(sys_cpu_buf), "%.1f%%", static_cast<double>(scene.system_cpu_percent));

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(width), static_cast<double>(height), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_LIGHTING);

    // Panel layout — top-left metrics panel
    const float panel_w = 280.0F;
    const float sparkline_h = 40.0F;
    const float header_h = 24.0F;
    const float row_h = 18.0F;
    constexpr int num_lines = 10;
    const float panel_height = header_h + static_cast<float>(num_lines) * row_h + sparkline_h + 16.0F;

    // Background panel
    draw_panel(8.0F, 8.0F, panel_w, panel_height, 0.04F, 0.06F, 0.09F, 0.82F);
    draw_panel_outline(8.0F, 8.0F, panel_w, panel_height, 0.18F, 0.30F, 0.48F, 0.7F);

    // Title header
    draw_ui_text(16.0F, 16.0F, 2.0F, "PERFORMANCE", UiTextStyle::Header);

    float y = 36.0F;
    draw_ui_text(16.0F, y, 1.7F, fps_buf, UiTextStyle::Section);
    glColor3f(ft_r, ft_g, ft_b);
    draw_text(108.0F, y, 1.7F, frame_buf);
    y += row_h;
    draw_ui_text(16.0F, y, 1.6F, format_overlay_line("CPU", scene.process_cpu_percent, "%").c_str(), UiTextStyle::Body);
    y += row_h;
    draw_ui_text(16.0F, y, 1.6F, format_overlay_line("SYS", scene.system_cpu_percent, "%").c_str(), UiTextStyle::Body);
    y += row_h;
    draw_ui_text(16.0F, y, 1.6F,
                 format_memory_line("RSS", scene.process_rss_mb, scene.system_total_memory_mb).c_str(),
                 UiTextStyle::Body);
    y += row_h;
    draw_ui_text(16.0F, y, 1.6F, format_integer_overlay_line("MAP CELLS", static_cast<double>(scene.render_stats.map_cells_visible), "").c_str(), UiTextStyle::Muted);
    y += row_h;
    draw_ui_text(16.0F, y, 1.6F, format_integer_overlay_line("DUMMIES", static_cast<double>(scene.render_stats.drawn_dummies), "").c_str(), UiTextStyle::Muted);
    y += row_h;
    draw_ui_text(16.0F, y, 1.6F, format_integer_overlay_line("DECALS", static_cast<double>(scene.render_stats.drawn_decal_count), "").c_str(), UiTextStyle::Muted);
    y += row_h;
    draw_ui_text(16.0F, y, 1.6F, format_integer_overlay_line("PARTICLES", static_cast<double>(scene.render_stats.drawn_particle_count), "").c_str(), UiTextStyle::Muted);
    y += row_h;
    draw_ui_text(16.0F, y, 1.6F, format_integer_overlay_line("LOD 0/1/2", static_cast<double>(scene.render_stats.lod0_count), "").c_str(), UiTextStyle::Muted);
    draw_ui_text(92.0F, y, 1.6F, format_integer_overlay_line("", static_cast<double>(scene.render_stats.lod1_count), "").c_str(), UiTextStyle::Muted);
    draw_ui_text(112.0F, y, 1.6F, format_integer_overlay_line("", static_cast<double>(scene.render_stats.lod2_count), "").c_str(), UiTextStyle::Muted);
    y += row_h;

    // --- Sparkline (frame-time history) ---
    const float spark_x = 16.0F;
    const float spark_y = y + 4.0F;
    const float spark_w = panel_w - 32.0F;
    const float spark_h = sparkline_h;

    // Sparkline background
    draw_panel(spark_x, spark_y, spark_w, spark_h, 0.06F, 0.08F, 0.12F, 0.7F);

    auto ms_to_y = [&](double ms) -> float {
        // Clamp to reasonable range (0—33 ms, i.e. ~30 fps floor)
        double clamped = ms;
        if (clamped < 0.0) clamped = 0.0;
        if (clamped > 33.0) clamped = 33.0;
        return spark_y + spark_h - static_cast<float>(clamped / 33.0) * spark_h;
    };

    // 8.3 ms and 16.7 ms reference lines (120 fps & 60 fps)
    float y_8 = ms_to_y(8.3);
    glColor4f(0.25F, 0.88F, 0.35F, 0.3F);
    glBegin(GL_LINES);
    glVertex2f(spark_x, y_8); glVertex2f(spark_x + spark_w, y_8);
    glEnd();

    float y_16 = ms_to_y(16.7);
    glColor4f(0.95F, 0.72F, 0.15F, 0.3F);
    glBegin(GL_LINES);
    glVertex2f(spark_x, y_16); glVertex2f(spark_x + spark_w, y_16);
    glEnd();

    // Compute min/max of visible history
    double hist_min = 1e9;
    double hist_max = 0.0;
    for (int i = 0; i < sparkline_count; ++i) {
        double v = frame_time_history[static_cast<std::size_t>(i)];
        if (v < hist_min) hist_min = v;
        if (v > hist_max) hist_max = v;
    }
    if (hist_max < 0.5) hist_max = 16.7;

    // Auto-scale: show the most interesting range
    double range = hist_max - hist_min;
    if (range < 4.0) range = 4.0;
    float scale_y = spark_h / static_cast<float>(range);
    float base_y = spark_y + spark_h;

    glLineWidth(1.5F);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < sparkline_count; ++i) {
        double v = frame_time_history[static_cast<std::size_t>(i)];
        float sx = spark_x + static_cast<float>(i) * spark_w / static_cast<float>(sparkline_count - 1 > 0 ? sparkline_count - 1 : 1);
        float sy = base_y - static_cast<float>(v - hist_min) * scale_y;
        // Color gradient: green → yellow → red
        float t = (v < 8.3) ? 0.0F : ((v < 16.7) ? static_cast<float>((v - 8.3) / 8.4) : 1.0F);
        glColor4f(0.25F + t * 0.70F, 0.88F - t * 0.63F, 0.35F - t * 0.15F, 0.9F);
        glVertex2f(sx, sy);
    }
    glEnd();
    glLineWidth(1.0F);

    // FPS label on sparkline
    char spark_label[32];
    double avg_ms = 0.0;
    if (sparkline_count > 0) {
        for (int i = 0; i < sparkline_count; ++i) avg_ms += frame_time_history[static_cast<std::size_t>(i)];
        avg_ms /= sparkline_count;
    }
    std::snprintf(spark_label, sizeof(spark_label), "%.1f ms avg", avg_ms);
    float y_label = spark_y + 12.0F;
    draw_ui_text(spark_x + 4.0F, y_label, 1.4F, spark_label, UiTextStyle::Muted);

    // Helper to draw a reference line with label
    auto draw_ref_line = [&](float y, const char* label_text, float r, float g, float b) {
        float label_w = 28.0F;
        glColor4f(r, g, b, 0.4F);
        glBegin(GL_LINES);
        glVertex2f(spark_x + label_w, y); glVertex2f(spark_x + spark_w, y);
        glEnd();
        draw_ui_text(spark_x + 2.0F, y - 7.0F, 1.1F, label_text, UiTextStyle::Muted);
    };
    draw_ref_line(y_8,  "120", 0.25F, 0.88F, 0.35F);
    draw_ref_line(y_16, "60",  0.95F, 0.72F, 0.15F);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void draw_gpu_profiler_overlay(const DebugScene& scene, int width, int height) {
    if (!scene.gpu_usage_available) return;

    const float panel_w = 240.0F;
    const float panel_h = 180.0F;
    const float panel_x = static_cast<float>(width) - panel_w - 16.0F;
    const float panel_y = static_cast<float>(height) - panel_h - 64.0F;
    const float bar_area_x = panel_x + 16.0F;
    const float bar_area_w = panel_w - 32.0F;
    const float bar_area_y = panel_y + 32.0F;
    const float bar_h = 14.0F;
    const float bar_gap = 6.0F;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(width), static_cast<double>(height), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Panel background
    glColor4f(0.04F, 0.06F, 0.09F, 0.92F);
    glBegin(GL_QUADS);
    draw_screen_quad(panel_x, panel_y, panel_w, panel_h);
    glEnd();

    // Header bar
    glColor4f(0.16F, 0.28F, 0.44F, 1.0F);
    glBegin(GL_QUADS);
    draw_screen_quad(panel_x, panel_y, panel_w, 28.0F);
    glEnd();

    draw_ui_text(panel_x + 12.0F, panel_y + 8.0F, 2.0F, "GPU PROFILER", UiTextStyle::Header);

    // Bar chart: stacked horizontal bars showing GPU time breakdown
    auto draw_bar = [&](float y_pos, const char* label, double time_ms, float r, float g, float b) {
        // Label
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%s", label);
        draw_ui_text(bar_area_x, y_pos, 2.0F, buf, UiTextStyle::Body);

        // Value
        char val_buf[32];
        std::snprintf(val_buf, sizeof(val_buf), "%.2f ms", time_ms);
        draw_ui_text(bar_area_x + bar_area_w - 70.0F, y_pos, 2.0F, val_buf, UiTextStyle::Muted);

        // Bar (width proportional to total frame budget of 33ms)
        float bar_w = bar_area_w * static_cast<float>(time_ms / 33.0);
        if (bar_w > bar_area_w) bar_w = bar_area_w;
        if (bar_w < 2.0F) bar_w = 2.0F;
        float bar_actual_y = y_pos + 18.0F;
        glColor4f(r * 0.2F, g * 0.2F, b * 0.2F, 0.6F);
        glBegin(GL_QUADS);
        draw_screen_quad(bar_area_x, bar_actual_y, bar_area_w, bar_h);
        glEnd();
        glColor4f(r, g, b, 0.9F);
        glBegin(GL_QUADS);
        draw_screen_quad(bar_area_x, bar_actual_y, bar_w, bar_h);
        glEnd();
    };

    float y = bar_area_y;
    draw_bar(y, "Depth",  scene.gpu_time_depth_ms,   0.35F, 0.55F, 0.85F);
    y += 28.0F;
    draw_bar(y, "Main",   scene.gpu_time_map_ms,     0.55F, 0.35F, 0.85F);
    y += 28.0F;
    draw_bar(y, "UI",     scene.gpu_time_ui_ms,      0.85F, 0.55F, 0.35F);
    y += 28.0F;

    // Total as a summary line
    y += 8.0F;
    draw_panel_outline(bar_area_x, y, bar_area_w, bar_h, 0.8F, 0.75F, 0.2F, 0.7F);
    draw_bar(y, "TOTAL", scene.gpu_time_total_ms, 0.95F, 0.85F, 0.20F);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

}  // namespace ae::render
