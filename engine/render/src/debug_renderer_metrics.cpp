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

namespace {

struct MatrixSnapshot {
    ae::gl_compat::Mat4 projection;
    ae::gl_compat::Mat4 modelview;
};

MatrixSnapshot begin_screen_space(int width, int height) {
    auto& st = ae::gl_compat::state();
    MatrixSnapshot snapshot {st.projection, st.modelview};
    st.projection = ae::gl_compat::mat4_ortho(0.0F, static_cast<float>(width),
                                              static_cast<float>(height), 0.0F,
                                              -1.0F, 1.0F);
    st.modelview = ae::gl_compat::Mat4::identity();
    return snapshot;
}

void end_screen_space(const MatrixSnapshot& snapshot) {
    auto& st = ae::gl_compat::state();
    st.projection = snapshot.projection;
    st.modelview = snapshot.modelview;
}

}  // namespace

namespace {

// Draw a small horizontal usage bar (e.g. for memory budget)
void draw_mini_bar(float x, float y, float w, float h, double fraction,
                    float r_ok, float g_ok, float b_ok,
                    float r_warn, float g_warn, float b_warn) {
    const float f = static_cast<float>(fraction);
    const float clamped = (f < 0.0F) ? 0.0F : (f > 1.0F ? 1.0F : f);
    // Color interpolate green->yellow->red
    float r, g, b;
    if (clamped < 0.6F) {
        float t = clamped / 0.6F;
        r = r_ok + (r_warn - r_ok) * t;
        g = g_ok + (g_warn - g_ok) * t;
        b = b_ok + (b_warn - b_ok) * t;
    } else {
        float t = (clamped - 0.6F) / 0.4F;
        r = r_warn + (1.0F - r_warn) * t;
        g = g_warn * (1.0F - t);
        b = b_warn * (1.0F - t);
    }
    // Background
    glColor4f(0.08F, 0.10F, 0.14F, 0.7F);
    draw_screen_quad(x, y, w, h);
    // Fill
    if (clamped > 0.0F) {
        glColor4f(r, g, b, 0.85F);
        draw_screen_quad(x, y, w * clamped, h);
    }
}

// Pressure color helpers
const char* pressure_label(std::uint8_t pressure) {
    switch (pressure) {
        case 0:  return "OK";
        case 1:  return "WARN";
        case 2:  return "CRIT";
        default: return "?";
    }
}

void set_pressure_color(std::uint8_t pressure) {
    switch (pressure) {
        case 0:  glColor3f(0.25F, 0.88F, 0.35F); break;  // green
        case 1:  glColor3f(0.95F, 0.72F, 0.15F); break;   // yellow
        case 2:  glColor3f(0.95F, 0.25F, 0.20F); break;   // red
        default: glColor3f(0.62F, 0.68F, 0.76F); break;
    }
}

}  // namespace

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

    const MatrixSnapshot snapshot = begin_screen_space(width, height);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Panel layout — top-left metrics panel (taller for budget section)
    const float panel_w = 300.0F;  // slightly wider for budget data
    const float sparkline_h = 40.0F;
    const float header_h = 24.0F;
    const float row_h = 18.0F;
    const float section_h = 14.0F;  // spacing between sections
    constexpr int num_info_lines = 14;  // frame + cpu + rss + budget + draw stats + allocator
    const float panel_height = header_h + static_cast<float>(num_info_lines) * row_h + sparkline_h + 32.0F + section_h * 2.0F;

    // Background panel
    draw_panel(8.0F, 8.0F, panel_w, panel_height, 0.04F, 0.06F, 0.09F, 0.85F);
    draw_panel_outline(8.0F, 8.0F, panel_w, panel_height, 0.18F, 0.30F, 0.48F, 0.7F);

    // Title header
    draw_ui_text(16.0F, 16.0F, 2.0F, "PERFORMANCE", UiTextStyle::Header);

    float y = 36.0F;

    // === Frame / FPS ===
    draw_ui_text(16.0F, y, 1.7F, fps_buf, UiTextStyle::Section);
    glColor3f(ft_r, ft_g, ft_b);
    draw_text(108.0F, y, 1.7F, frame_buf);

    // Budget compliance chip
    char compliance_buf[32];
    std::snprintf(compliance_buf, sizeof(compliance_buf), "%.0f%%", scene.frame_budget_compliance * 100.0);
    draw_ui_text(202.0F, y, 1.4F, compliance_buf,
                 scene.frame_pacing_healthy ? UiTextStyle::Body : UiTextStyle::Accent);
    y += row_h;

    // 1% low / rolling avg
    char p1_buf[64];
    std::snprintf(p1_buf, sizeof(p1_buf), "P1: %.1f  AVG: %.1f",
                  scene.frame_p1_low_ms, scene.frame_rolling_avg_ms);
    draw_ui_text(16.0F, y, 1.4F, p1_buf, UiTextStyle::Muted);

    // Pacing health indicator
    set_pressure_color(scene.frame_pacing_healthy ? 0 : (scene.frame_regression ? 2 : 1));
    draw_text(210.0F, y, 1.4F, scene.frame_pacing_healthy ? "OK" : (scene.frame_regression ? "REGR" : "SLOW"));
    y += row_h;

    // CPU
    draw_ui_text(16.0F, y, 1.6F, format_overlay_line("CPU", scene.process_cpu_percent, "%").c_str(), UiTextStyle::Body);
    y += row_h;

    y += section_h;

    // === Memory Budget Section ===
    draw_ui_text(16.0F, y, 1.4F, "MEMORY", UiTextStyle::Section);
    y += row_h;

    // RSS    const double rss_mb = scene.rss_bytes / (1024.0 * 1024.0);
    const double rss_peak_mb = scene.rss_peak_bytes / (1024.0 * 1024.0);
    const double rss_soft_mb = scene.rss_soft_budget / (1024.0 * 1024.0);
    const double rss_hard_mb = scene.rss_hard_budget / (1024.0 * 1024.0);
    char rss_buf[64];
    std::snprintf(rss_buf, sizeof(rss_buf), "RSS %.0f/%.0f (pk %.0f)", rss_mb, rss_hard_mb, rss_peak_mb);
    draw_ui_text(16.0F, y, 1.5F, rss_buf, UiTextStyle::Body);
    set_pressure_color(scene.rss_pressure);
    draw_text(216.0F, y, 1.3F, pressure_label(scene.rss_pressure));
    y += row_h;

    // RSS mini-bar
    if (scene.rss_hard_budget > 0.0) {
        draw_mini_bar(16.0F, y, panel_w - 32.0F, 5.0F,
                      rss_mb / rss_hard_mb, 0.25F, 0.88F, 0.35F, 0.95F, 0.72F, 0.15F);
        y += 10.0F;
    } else {
        y += 2.0F;
    }

    // Frame allocator usage
    if (scene.frame_alloc_capacity_bytes > 0.0) {
        const double alloc_mb = scene.frame_alloc_peak_bytes / (1024.0 * 1024.0);
        const double alloc_cap_mb = scene.frame_alloc_capacity_bytes / (1024.0 * 1024.0);
        char alloc_buf[64];
        std::snprintf(alloc_buf, sizeof(alloc_buf), "ALLOC %.1f/%.1f MB", alloc_mb, alloc_cap_mb);
        draw_ui_text(16.0F, y, 1.5F, alloc_buf, UiTextStyle::Body);
        set_pressure_color(scene.frame_alloc_pressure);
        draw_text(216.0F, y, 1.3F, pressure_label(scene.frame_alloc_pressure));
        y += row_h;

        draw_mini_bar(16.0F, y, panel_w - 32.0F, 5.0F,
                      alloc_mb / alloc_cap_mb, 0.25F, 0.88F, 0.35F, 0.95F, 0.72F, 0.15F);
        y += 10.0F;
    }

    y += section_h;

    // === Draw Stats Section ===
    draw_ui_text(16.0F, y, 1.4F, "DRAW", UiTextStyle::Section);
    y += row_h;
    draw_ui_text(16.0F, y, 1.6F, format_integer_overlay_line("MAP", static_cast<double>(scene.render_stats.map_cells_visible), "").c_str(), UiTextStyle::Muted);
    y += row_h;
    draw_ui_text(16.0F, y, 1.6F, format_integer_overlay_line("DMY", static_cast<double>(scene.render_stats.drawn_dummies), "").c_str(), UiTextStyle::Muted);
    y += row_h;
    draw_ui_text(16.0F, y, 1.6F, format_integer_overlay_line("DEC", static_cast<double>(scene.render_stats.drawn_decal_count), "").c_str(), UiTextStyle::Muted);
    y += row_h;
    draw_ui_text(16.0F, y, 1.6F, format_integer_overlay_line("PRT", static_cast<double>(scene.render_stats.drawn_particle_count), "").c_str(), UiTextStyle::Muted);
    y += row_h;
    draw_ui_text(16.0F, y, 1.6F, format_integer_overlay_line("LOD", static_cast<double>(scene.render_stats.lod0_count), "").c_str(), UiTextStyle::Muted);
    draw_ui_text(92.0F, y, 1.6F, format_integer_overlay_line("", static_cast<double>(scene.render_stats.lod1_count), "").c_str(), UiTextStyle::Muted);
    draw_ui_text(112.0F, y, 1.6F, format_integer_overlay_line("", static_cast<double>(scene.render_stats.lod2_count), "").c_str(), UiTextStyle::Muted);
    y += row_h;

    y += section_h;

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

    // Budget target line (dynamic from scene)
    float y_budget = ms_to_y(scene.frame_budget_ms);
    glColor4f(0.25F, 0.88F, 0.35F, 0.4F);
    draw_screen_line(spark_x, y_budget, spark_x + spark_w, y_budget);

    // 8.3 ms and 16.7 ms reference lines (120 fps & 60 fps)
    float y_8 = ms_to_y(8.3);
    glColor4f(0.25F, 0.88F, 0.35F, 0.3F);
    draw_screen_line(spark_x, y_8, spark_x + spark_w, y_8);

    float y_16 = ms_to_y(16.7);
    glColor4f(0.95F, 0.72F, 0.15F, 0.3F);
    draw_screen_line(spark_x, y_16, spark_x + spark_w, y_16);

    // Compute min/max of visible history
    double hist_min = 1e9;
    double hist_max = 0.0;
    for (int i = 0; i < sparkline_count; ++i) {
        double v = frame_time_history[static_cast<std::size_t>(i)];
        if (v < hist_min) hist_min = v;
        if (v > hist_max) hist_max = v;
    }
    if (hist_max < 0.5) hist_max = scene.frame_budget_ms > 0.0 ? scene.frame_budget_ms : 16.7;

    // Auto-scale: show the most interesting range
    double range = hist_max - hist_min;
    if (range < 4.0) range = 4.0;
    float scale_y = spark_h / static_cast<float>(range);
    float base_y = spark_y + spark_h;

    glLineWidth(1.5F);
    if (sparkline_count > 1) {
        float prev_x = spark_x;
        float prev_y = base_y - static_cast<float>(frame_time_history[0] - hist_min) * scale_y;
        for (int i = 1; i < sparkline_count; ++i) {
            double v = frame_time_history[static_cast<std::size_t>(i)];
            float sx = spark_x + static_cast<float>(i) * spark_w / static_cast<float>(sparkline_count - 1);
            float sy = base_y - static_cast<float>(v - hist_min) * scale_y;
            float t = (v < 8.3) ? 0.0F : ((v < 16.7) ? static_cast<float>((v - 8.3) / 8.4) : 1.0F);
            glColor4f(0.25F + t * 0.70F, 0.88F - t * 0.63F, 0.35F - t * 0.15F, 0.9F);
            draw_screen_line(prev_x, prev_y, sx, sy);
            prev_x = sx;
            prev_y = sy;
        }
    }
    glLineWidth(1.0F);

    // Labels on sparkline
    char spark_label[48];
    double avg_ms = 0.0;
    if (sparkline_count > 0) {
        for (int i = 0; i < sparkline_count; ++i) avg_ms += frame_time_history[static_cast<std::size_t>(i)];
        avg_ms /= sparkline_count;
    }
    std::snprintf(spark_label, sizeof(spark_label), "%.1f ms avg | budget %.0f", avg_ms, scene.frame_budget_ms);
    float y_label = spark_y + 12.0F;
    draw_ui_text(spark_x + 4.0F, y_label, 1.4F, spark_label, UiTextStyle::Muted);

    // Helper to draw a reference line with label
    auto draw_ref_line = [&](float ref_y, const char* label_text, float r, float g, float b) {
        float label_w = 28.0F;
        glColor4f(r, g, b, 0.4F);
        draw_screen_line(spark_x + label_w, ref_y, spark_x + spark_w, ref_y);
        draw_ui_text(spark_x + 2.0F, ref_y - 7.0F, 1.1F, label_text, UiTextStyle::Muted);
    };
    draw_ref_line(y_8,  "120", 0.25F, 0.88F, 0.35F);
    draw_ref_line(y_16, "60",  0.95F, 0.72F, 0.15F);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    end_screen_space(snapshot);
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

    const MatrixSnapshot snapshot = begin_screen_space(width, height);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Panel background
    glColor4f(0.04F, 0.06F, 0.09F, 0.92F);
    draw_screen_quad(panel_x, panel_y, panel_w, panel_h);

    // Header bar
    glColor4f(0.16F, 0.28F, 0.44F, 1.0F);
    draw_screen_quad(panel_x, panel_y, panel_w, 28.0F);

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
        draw_screen_quad(bar_area_x, bar_actual_y, bar_area_w, bar_h);
        glColor4f(r, g, b, 0.9F);
        draw_screen_quad(bar_area_x, bar_actual_y, bar_w, bar_h);
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

    end_screen_space(snapshot);
}

}  // namespace ae::render
