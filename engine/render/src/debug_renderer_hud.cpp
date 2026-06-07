// SPDX-License-Identifier: MIT
// HUD / debug overlay drawing for the debug renderer.
//
// Includes: crosshair, HUD (health bar, ammo, minimap, compass),
//           menu overlay, scene/objective overlay.

#include "debug_renderer_internal.h"

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdio>
#include <string>

namespace ae::render {

// ============================================================================
// Crosshair overlay
// ============================================================================

void draw_crosshair_overlay(const DebugScene& scene, int width, int height) {
    const float center_x = static_cast<float>(width) * 0.5F;
    const float center_y = static_cast<float>(height) * 0.5F;
    constexpr float arm_length = 8.0F;
    constexpr float gap = 4.0F;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(width), static_cast<double>(height), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glLineWidth(2.0F);
    glColor3f(1.0F, 0.9F, 0.1F);  // Bright yellow crosshair
    glBegin(GL_LINES);
    glVertex2f(center_x - arm_length, center_y);
    glVertex2f(center_x - gap, center_y);
    glVertex2f(center_x + gap, center_y);
    glVertex2f(center_x + arm_length, center_y);
    glVertex2f(center_x, center_y - arm_length);
    glVertex2f(center_x, center_y - gap);
    glVertex2f(center_x, center_y + gap);
    glVertex2f(center_x, center_y + arm_length);
    glEnd();

    if (scene.hitmarker_time > 0.0F) {
        float alpha = scene.hitmarker_time;
        if (scene.hitmarker_is_critical) {
            glColor4f(1.0F, 0.15F, 0.15F, alpha);
        } else {
            glColor4f(1.0F, 1.0F, 1.0F, alpha);
        }
        glLineWidth(2.0F);
        constexpr float h_gap = 6.0F;
        constexpr float h_size = 12.0F;
        glBegin(GL_LINES);
        // Top-left
        glVertex2f(center_x - h_gap, center_y - h_gap);
        glVertex2f(center_x - h_size, center_y - h_size);
        // Top-right
        glVertex2f(center_x + h_gap, center_y - h_gap);
        glVertex2f(center_x + h_size, center_y - h_size);
        // Bottom-left
        glVertex2f(center_x - h_gap, center_y + h_gap);
        glVertex2f(center_x - h_size, center_y + h_size);
        // Bottom-right
        glVertex2f(center_x + h_gap, center_y + h_gap);
        glVertex2f(center_x + h_size, center_y + h_size);

        if (scene.hitmarker_is_critical) {
            // Draw neat brackets around the reticle
            constexpr float b_offset = 15.0F;
            constexpr float b_len = 5.0F;
            // Top-left bracket
            glVertex2f(center_x - b_offset, center_y - b_offset);
            glVertex2f(center_x - b_offset + b_len, center_y - b_offset);
            glVertex2f(center_x - b_offset, center_y - b_offset);
            glVertex2f(center_x - b_offset, center_y - b_offset + b_len);
            // Top-right bracket
            glVertex2f(center_x + b_offset, center_y - b_offset);
            glVertex2f(center_x + b_offset - b_len, center_y - b_offset);
            glVertex2f(center_x + b_offset, center_y - b_offset);
            glVertex2f(center_x + b_offset, center_y - b_offset + b_len);
            // Bottom-left bracket
            glVertex2f(center_x - b_offset, center_y + b_offset);
            glVertex2f(center_x - b_offset + b_len, center_y + b_offset);
            glVertex2f(center_x - b_offset, center_y + b_offset);
            glVertex2f(center_x - b_offset, center_y + b_offset - b_len);
            // Bottom-right bracket
            glVertex2f(center_x + b_offset, center_y + b_offset);
            glVertex2f(center_x + b_offset - b_len, center_y + b_offset);
            glVertex2f(center_x + b_offset, center_y + b_offset);
            glVertex2f(center_x + b_offset, center_y + b_offset - b_len);
        }
        glEnd();
    }

    glLineWidth(1.0F);
    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// ============================================================================
// HUD (health bar, ammo counter, minimap, compass)
// ============================================================================

void draw_hud(const DebugScene& scene, int width, int height, float hud_brightness) {
    // --- Screen-space setup (health bar + ammo) ---
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

    // --- Health Bar (bottom-center) ---
    constexpr float hb_w = 320.0F;
    constexpr float hb_h = 10.0F;
    const float hb_x = static_cast<float>(width) * 0.5F - hb_w * 0.5F;
    const float hb_y = static_cast<float>(height) - 45.0F;

    float health_pct = scene.player_max_health > 0.0F
                           ? scene.player_health / scene.player_max_health
                           : 0.0F;
    if (health_pct > 1.0F) health_pct = 1.0F;
    if (health_pct < 0.0F) health_pct = 0.0F;

    const bool low_health = health_pct <= 0.3F;
    float pulse = 1.0F;
    if (low_health) {
        pulse = 0.6F + 0.4F * std::sin(static_cast<float>(glfwGetTime()) * 12.0F);
    }

    // HP Label (Top-Left)
    draw_ui_text(hb_x, hb_y - 15.0F, 1.5F, "HP", low_health ? UiTextStyle::Accent : UiTextStyle::Header);

    // Numeric value (Top-Right)
    char health_buf[32]{};
    std::snprintf(health_buf, sizeof(health_buf), "%.0f / %.0f",
                  static_cast<double>(scene.player_health),
                  static_cast<double>(scene.player_max_health));
    FontAtlas& font_atlas = shared_ui_font_atlas();
    const float value_w = font_atlas.measure_text(health_buf, 1.4F);
    draw_ui_text(hb_x + hb_w - value_w, hb_y - 15.0F, 1.4F, health_buf, low_health ? UiTextStyle::Accent : UiTextStyle::Header);

    // Draw Health Bar Background Panel (Glassmorphic dark slate)
    const float hb_alpha = 0.65F * hud_brightness;
    draw_panel(hb_x - 3.0F, hb_y - 3.0F, hb_w + 6.0F, hb_h + 6.0F, 0.04F, 0.06F, 0.09F, hb_alpha);
    if (low_health) {
        draw_panel_outline(hb_x - 3.0F, hb_y - 3.0F, hb_w + 6.0F, hb_h + 6.0F, 0.85F, 0.15F * pulse, 0.15F * pulse, 0.9F * hud_brightness);
    } else {
        draw_panel_outline(hb_x - 3.0F, hb_y - 3.0F, hb_w + 6.0F, hb_h + 6.0F, 0.22F, 0.34F, 0.52F, 0.75F * hud_brightness);
    }

    // Segmented health bar fill
    if (health_pct > 0.0F) {
        float bar_r = 0.2F, bar_g = 0.85F, bar_b = 0.2F;
        if (health_pct <= 0.3F) {
            bar_r = 0.85F; bar_g = 0.15F * pulse; bar_b = 0.15F * pulse;
        } else if (health_pct <= 0.6F) {
            bar_r = 0.85F; bar_g = 0.85F; bar_b = 0.15F;
        }

        constexpr int num_segments = 20;
        const float segment_spacing = 2.0F;
        const float total_spacing_w = segment_spacing * (num_segments - 1);
        const float segment_w = (hb_w - total_spacing_w) / num_segments;

        glColor4f(bar_r, bar_g, bar_b, 0.9F * hud_brightness);
        glBegin(GL_QUADS);
        for (int i = 0; i < num_segments; ++i) {
            const float seg_x = hb_x + static_cast<float>(i) * (segment_w + segment_spacing);
            const float seg_mid = (static_cast<float>(i) + 0.5F) / static_cast<float>(num_segments);
            if (seg_mid <= health_pct) {
                draw_screen_quad(seg_x, hb_y, segment_w, hb_h);
            }
        }
        glEnd();
    }

    draw_xbox_button_legend(scene.controller_buttons, hb_x + hb_w + 24.0F, hb_y - 30.0F);

    // --- Ammo Counter (bottom-left card) ---
    const float card_x = 24.0F;
    const float card_w = 190.0F;
    const float card_h = 76.0F;
    const float card_y = static_cast<float>(height) - card_h - 24.0F;

    float ammo_pct = scene.ammo_max > 0.0F ? scene.ammo_current / scene.ammo_max : 0.0F;
    if (ammo_pct > 1.0F) ammo_pct = 1.0F;
    if (ammo_pct < 0.0F) ammo_pct = 0.0F;

    const bool low_ammo = scene.ammo_current <= 5.0F;
    float ammo_pulse = 1.0F;
    if (low_ammo) {
        ammo_pulse = 0.6F + 0.4F * std::sin(static_cast<float>(glfwGetTime()) * 10.0F);
    }

    const float card_alpha = 0.65F * hud_brightness;
    draw_panel(card_x, card_y, card_w, card_h, 0.04F, 0.06F, 0.09F, card_alpha);
    if (low_ammo) {
        draw_panel_outline(card_x, card_y, card_w, card_h, 0.85F, 0.15F * ammo_pulse, 0.15F * ammo_pulse, 0.85F * hud_brightness);
    } else {
        draw_panel_outline(card_x, card_y, card_w, card_h, 0.96F, 0.84F, 0.16F, 0.7F * hud_brightness);
    }

    char curr_ammo_buf[16]{};
    std::snprintf(curr_ammo_buf, sizeof(curr_ammo_buf), "%.0f", static_cast<double>(scene.ammo_current));
    draw_ui_text(card_x + 16.0F, card_y + 12.0F, 3.4F, curr_ammo_buf, low_ammo ? UiTextStyle::Accent : UiTextStyle::Header);

    char reserve_buf[16]{};
    std::snprintf(reserve_buf, sizeof(reserve_buf), "/ %.0f", static_cast<double>(scene.ammo_max));
    draw_ui_text(card_x + 72.0F, card_y + 24.0F, 1.8F, reserve_buf, UiTextStyle::Muted);

    draw_ui_text(card_x + 16.0F, card_y + 52.0F, 1.1F, "STANDARD RIFLE", UiTextStyle::Muted);

    const float mag_bar_x = card_x + 16.0F;
    const float mag_bar_y = card_y + 44.0F;
    const float mag_bar_w = card_w - 32.0F;
    const float mag_bar_h = 3.0F;

    draw_panel(mag_bar_x, mag_bar_y, mag_bar_w, mag_bar_h, 0.1F, 0.12F, 0.15F, 1.0F * hud_brightness);
    if (ammo_pct > 0.0F) {
        if (low_ammo) {
            glColor4f(0.85F, 0.15F * ammo_pulse, 0.15F * ammo_pulse, 0.9F * hud_brightness);
        } else {
            glColor4f(0.96F, 0.84F, 0.16F, 0.9F * hud_brightness);
        }
        glBegin(GL_QUADS);
        draw_screen_quad(mag_bar_x, mag_bar_y, mag_bar_w * ammo_pct, mag_bar_h);
        glEnd();
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    // --- Minimap (bottom-right, separate world-space ortho) ---
    constexpr float mm_size = 150.0F;
    constexpr float mm_margin = 10.0F;
    const int mm_x = width - static_cast<int>(mm_size) - static_cast<int>(mm_margin);
    const int mm_y = height - static_cast<int>(mm_size) - static_cast<int>(mm_margin);

    glViewport(mm_x, mm_y, static_cast<int>(mm_size), static_cast<int>(mm_size));

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    // Map 30x30m arena (-15..15) to circular radar
    constexpr double world_extent = 16.0;
    glOrtho(-world_extent, world_extent, world_extent, -world_extent, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Dark semi-transparent background circle
    constexpr float r_inner = 14.5F;
    glColor4f(0.02F, 0.04F, 0.08F, 0.75F);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(0.0F, 0.0F);
    for (int step = 0; step <= 36; ++step) {
        const float angle = static_cast<float>(step) / 36.0F * 2.0F * kPi;
        glVertex2f(std::cos(angle) * r_inner, std::sin(angle) * r_inner);
    }
    glEnd();

    // Arena boundary outline (white)
    glColor3f(0.6F, 0.6F, 0.6F);
    glLineWidth(1.0F);
    glBegin(GL_LINE_LOOP);
    glVertex2f(-15.0F, -15.0F);
    glVertex2f(15.0F, -15.0F);
    glVertex2f(15.0F, 15.0F);
    glVertex2f(-15.0F, 15.0F);
    glEnd();

    // Central platform (blue square, +/-4)
    glColor3f(0.18F, 0.38F, 0.78F);
    glBegin(GL_QUADS);
    glVertex2f(-4.0F, -4.0F);
    glVertex2f(4.0F, -4.0F);
    glVertex2f(4.0F, 4.0F);
    glVertex2f(-4.0F, 4.0F);
    glEnd();

    // Alpha Spawn (gray, X=-13..-10, Z=-3..3)
    glColor3f(0.35F, 0.35F, 0.35F);
    glBegin(GL_QUADS);
    glVertex2f(-13.0F, -3.0F);
    glVertex2f(-10.0F, -3.0F);
    glVertex2f(-10.0F, 3.0F);
    glVertex2f(-13.0F, 3.0F);
    glEnd();

    // Bravo Spawn (gray, X=10..13, Z=-3..3)
    glBegin(GL_QUADS);
    glVertex2f(10.0F, -3.0F);
    glVertex2f(13.0F, -3.0F);
    glVertex2f(13.0F, 3.0F);
    glVertex2f(10.0F, 3.0F);
    glEnd();

    // Draw projectiles on the radar (tactical orange pings)
    for (int i = 0; i < scene.projectile_count && i < 64; ++i) {
        const auto& pp = scene.projectile_positions[i];
        const float dist = std::sqrt(pp.x * pp.x + pp.z * pp.z);
        if (dist < r_inner) {
            glColor4f(1.0F, 0.55F, 0.1F, 0.85F);
            glPointSize(4.0F);
            glBegin(GL_POINTS);
            glVertex2f(pp.x, pp.z);
            glEnd();
        }
    }
    glPointSize(1.0F);

    // Radar sweep line with fade trailing
    const float sweep_angle = -static_cast<float>(glfwGetTime()) * 1.5F;
    glBegin(GL_LINES);
    for (int i = 0; i < 5; ++i) {
        float angle = sweep_angle + static_cast<float>(i) * 0.05F;
        float alpha = 0.4F * (1.0F - static_cast<float>(i) / 5.0F);
        glColor4f(0.0F, 1.0F, 0.4F, alpha);
        glVertex2f(0.0F, 0.0F);
        glVertex2f(std::cos(angle) * r_inner, std::sin(angle) * r_inner);
    }
    glEnd();

    // Mask out the corners of the viewport to make the minimap circular
    glColor4f(0.05F, 0.07F, 0.11F, 1.0F);
    glBegin(GL_QUAD_STRIP);
    constexpr float r_outer = 30.0F;
    for (int step = 0; step <= 36; ++step) {
        const float angle = static_cast<float>(step) / 36.0F * 2.0F * kPi;
        const float cos_a = std::cos(angle);
        const float sin_a = std::sin(angle);
        glVertex2f(cos_a * r_inner, sin_a * r_inner);
        glVertex2f(cos_a * r_outer, sin_a * r_outer);
    }
    glEnd();

    // High-tech circular outer border
    glColor4f(0.22F, 0.5F, 0.85F, 0.8F);
    glLineWidth(2.0F);
    glBegin(GL_LINE_LOOP);
    for (int step = 0; step < 36; ++step) {
        const float angle = static_cast<float>(step) / 36.0F * 2.0F * kPi;
        glVertex2f(std::cos(angle) * r_inner, std::sin(angle) * r_inner);
    }
    glEnd();
    glLineWidth(1.0F);

    // Player directional arrow
    const float yaw = scene.player_yaw;
    const float arrow_len = 1.6F;
    const float arrow_width = 1.0F;
    float dx = std::sin(yaw);
    float dz = std::cos(yaw);
    float tx = -dz;
    float tz = dx;

    float px = scene.player_position.x;
    float pz = scene.player_position.z;
    float arrow_pulse = 0.8F + 0.2F * std::sin(static_cast<float>(glfwGetTime()) * 5.0F);

    glColor4f(0.0F, 1.0F, 0.3F, arrow_pulse);
    glBegin(GL_TRIANGLES);
    glVertex2f(px + dx * arrow_len, pz + dz * arrow_len);
    glVertex2f(px - dx * 0.8F + tx * arrow_width, pz - dz * 0.8F + tz * arrow_width);
    glVertex2f(px - dx * 0.8F - tx * arrow_width, pz - dz * 0.8F - tz * arrow_width);
    glEnd();

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    // Restore full viewport
    glViewport(0, 0, width, height);

    // Draw Compass letters in screen-space overlay (N, S, E, W)
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(width), static_cast<double>(height), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);

    const float mm_cx = static_cast<float>(mm_x) + mm_size * 0.5F;
    const float mm_cy = static_cast<float>(mm_y) + mm_size * 0.5F;
    draw_ui_text(mm_cx - 4.0F, mm_cy - 74.0F, 1.1F, "N", UiTextStyle::Header);
    draw_ui_text(mm_cx - 4.0F, mm_cy + 63.0F, 1.1F, "S", UiTextStyle::Header);
    draw_ui_text(mm_cx - 72.0F, mm_cy - 5.0F, 1.1F, "W", UiTextStyle::Header);
    draw_ui_text(mm_cx + 64.0F, mm_cy - 5.0F, 1.1F, "E", UiTextStyle::Header);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// ============================================================================
// Menu overlay
// ============================================================================

void draw_menu_overlay(const DebugScene& scene, int width, int height) {
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

    draw_panel(0.0F, 0.0F, static_cast<float>(width), static_cast<float>(height), 0.0F, 0.0F, 0.0F, 0.72F);

    const float panel_x = 90.0F;
    const float panel_y = 72.0F;
    const float panel_w = static_cast<float>(width) - 180.0F;
    const float panel_h = static_cast<float>(height) - 144.0F;
    const float sidebar_w = 210.0F;
    const float content_x = panel_x + sidebar_w + 24.0F;
    const float content_y = panel_y + 84.0F;

    draw_panel(panel_x, panel_y, panel_w, panel_h, 0.04F, 0.06F, 0.09F, 0.96F);
    draw_panel_outline(panel_x, panel_y, panel_w, panel_h, 0.18F, 0.30F, 0.48F, 0.9F);
    draw_panel(panel_x, panel_y, panel_w, 54.0F, 0.10F, 0.18F, 0.30F, 1.0F);
    draw_panel(panel_x, panel_y, sidebar_w, panel_h, 0.05F, 0.08F, 0.12F, 0.96F);

    draw_ui_text(panel_x + 24.0F, panel_y + 16.0F, 2.6F, "PLAYER MENU", UiTextStyle::Header);
    draw_ui_text(panel_x + panel_w - 170.0F, panel_y + 16.0F, 1.8F, "START: CLOSE", UiTextStyle::Muted);

    const char* tabs[] = {"CHARACTER", "SETTINGS"};
    for (int i = 0; i < 2; ++i) {
        const float tab_y = panel_y + 86.0F + static_cast<float>(i) * 52.0F;
        if (i == scene.menu_tab) {
            draw_panel(panel_x + 14.0F, tab_y - 10.0F, sidebar_w - 28.0F, 38.0F, 0.16F, 0.28F, 0.44F, 1.0F);
            draw_ui_text(panel_x + 34.0F, tab_y, 2.2F, tabs[i], UiTextStyle::Header);
        } else {
            draw_ui_text(panel_x + 34.0F, tab_y, 2.2F, tabs[i], UiTextStyle::Muted);
        }
    }

    draw_ui_text(panel_x + 28.0F, panel_y + panel_h - 54.0F, 1.8F, "LB / RB SWITCH TABS", UiTextStyle::Muted);
    draw_ui_text(panel_x + 28.0F, panel_y + panel_h - 30.0F, 1.8F, "BACK = METRICS", UiTextStyle::Muted);

    if (scene.menu_tab == 0) {
        draw_panel(content_x, content_y, panel_w - sidebar_w - 48.0F, 170.0F, 0.08F, 0.11F, 0.16F, 0.92F);
        draw_panel_outline(content_x, content_y, panel_w - sidebar_w - 48.0F, 170.0F, 0.22F, 0.34F, 0.52F, 0.85F);
        draw_ui_text(content_x + 20.0F, content_y + 16.0F, 2.4F, "LOADOUT", UiTextStyle::Header);

        char buf[64];
        std::snprintf(buf, sizeof(buf), "HP %.0f / %.0f", scene.player_health, scene.player_max_health);
        glColor3f(0.68F, 0.88F, 0.40F);
        draw_text(content_x + 24.0F, content_y + 54.0F, 2.2F, buf);

        std::snprintf(buf, sizeof(buf), "AMMO %.0f / %.0f", scene.ammo_current, scene.ammo_max);
        glColor3f(0.96F, 0.84F, 0.16F);
        draw_text(content_x + 24.0F, content_y + 84.0F, 2.2F, buf);

        draw_panel(content_x, content_y + 194.0F, panel_w - sidebar_w - 48.0F, 220.0F, 0.08F, 0.11F, 0.16F, 0.92F);
        draw_panel_outline(content_x, content_y + 194.0F, panel_w - sidebar_w - 48.0F, 220.0F, 0.22F, 0.34F, 0.52F, 0.85F);
        draw_ui_text(content_x + 20.0F, content_y + 210.0F, 2.4F, "GEAR", UiTextStyle::Section);
        draw_ui_text(content_x + 24.0F, content_y + 248.0F, 2.1F, "PRIMARY   STANDARD RIFLE", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 280.0F, 2.1F, "SPECIAL   SHOTGUN", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 312.0F, 2.1F, "HEAVY     ROCKET", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 356.0F, 1.9F, "MOBILITY  75", UiTextStyle::Body);
        draw_ui_text(content_x + 160.0F, content_y + 356.0F, 1.9F, "RESILIENCE 62", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 386.0F, 1.9F, "RECOVERY  58", UiTextStyle::Body);

        draw_panel(content_x + panel_w - sidebar_w - 220.0F, content_y + 24.0F, 140.0F, 140.0F, 0.11F, 0.15F, 0.22F, 1.0F);
        draw_panel_outline(content_x + panel_w - sidebar_w - 220.0F, content_y + 24.0F, 140.0F, 140.0F, 0.30F, 0.40F, 0.56F, 0.9F);
        draw_ui_text(content_x + panel_w - sidebar_w - 186.0F, content_y + 74.0F, 2.4F, "GUARDIAN", UiTextStyle::Section);
    } else {
        draw_panel(content_x, content_y, panel_w - sidebar_w - 48.0F, 150.0F, 0.08F, 0.11F, 0.16F, 0.92F);
        draw_panel_outline(content_x, content_y, panel_w - sidebar_w - 48.0F, 150.0F, 0.22F, 0.34F, 0.52F, 0.85F);
        draw_ui_text(content_x + 20.0F, content_y + 16.0F, 2.4F, "GRAPHICS", UiTextStyle::Header);
        draw_ui_text(content_x + 24.0F, content_y + 52.0F, 2.0F, "RESOLUTION   1920 X 1080", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 82.0F, 2.0F, "FULLSCREEN   OFF", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 112.0F, 2.0F, "FOV          90", UiTextStyle::Body);

        draw_panel(content_x, content_y + 174.0F, panel_w - sidebar_w - 48.0F, 126.0F, 0.08F, 0.11F, 0.16F, 0.92F);
        draw_panel_outline(content_x, content_y + 174.0F, panel_w - sidebar_w - 48.0F, 126.0F, 0.22F, 0.34F, 0.52F, 0.85F);
        draw_ui_text(content_x + 20.0F, content_y + 190.0F, 2.4F, "AUDIO", UiTextStyle::Header);
        draw_ui_text(content_x + 24.0F, content_y + 226.0F, 2.0F, "MASTER      80", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 256.0F, 2.0F, "SFX         90", UiTextStyle::Body);

        draw_panel(content_x, content_y + 324.0F, panel_w - sidebar_w - 48.0F, 190.0F, 0.08F, 0.11F, 0.16F, 0.92F);
        draw_panel_outline(content_x, content_y + 324.0F, panel_w - sidebar_w - 48.0F, 190.0F, 0.22F, 0.34F, 0.52F, 0.85F);
        draw_ui_text(content_x + 20.0F, content_y + 340.0F, 2.4F, "CONTROLS", UiTextStyle::Header);
        draw_ui_text(content_x + 24.0F, content_y + 376.0F, 2.0F, "MOVE        LEFT STICK", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 406.0F, 2.0F, "LOOK        RIGHT STICK", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 436.0F, 2.0F, "JUMP        A", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 466.0F, 2.0F, "FIRE        RT", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 496.0F, 2.0F, "RELOAD      Y", UiTextStyle::Body);

        draw_xbox_button_legend(scene.controller_buttons, content_x + panel_w - sidebar_w - 292.0F, content_y + 348.0F);
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// ============================================================================
// Scene / objective overlay
// ============================================================================

void draw_scene_overlay(const DebugScene& scene, int width, int height) {
    if (scene.overlay_title == nullptr && scene.overlay_body == nullptr && scene.overlay_hint == nullptr
        && scene.objective_text == nullptr) {
        return;
    }

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

    if (scene.overlay_title != nullptr || scene.overlay_body != nullptr || scene.overlay_hint != nullptr) {
        const float panel_w = std::min(620.0F, static_cast<float>(width) - 80.0F);
        const float panel_h = scene.overlay_body != nullptr ? 190.0F : 132.0F;
        const float panel_x = static_cast<float>(width) * 0.5F - panel_w * 0.5F;
        const float panel_y = static_cast<float>(height) * 0.5F - panel_h * 0.5F;

        draw_panel(0.0F, 0.0F, static_cast<float>(width), static_cast<float>(height), 0.0F, 0.0F, 0.0F, 0.46F);
        draw_panel(panel_x, panel_y, panel_w, panel_h, 0.035F, 0.055F, 0.080F, 0.94F);
        draw_panel_outline(panel_x, panel_y, panel_w, panel_h, 0.80F, 0.58F, 0.18F, 0.88F);

        if (scene.overlay_title != nullptr) {
            draw_ui_text(panel_x + 26.0F, panel_y + 24.0F, 3.8F, scene.overlay_title, UiTextStyle::Header);
        }
        if (scene.overlay_body != nullptr) {
            draw_ui_text(panel_x + 28.0F, panel_y + 86.0F, 2.1F, scene.overlay_body, UiTextStyle::Body);
        }
        if (scene.overlay_hint != nullptr) {
            draw_ui_text(panel_x + 28.0F, panel_y + panel_h - 42.0F, 2.0F, scene.overlay_hint, UiTextStyle::Accent);
        }
    }

    if (scene.objective_text != nullptr) {
        draw_panel(20.0F, 20.0F, 420.0F, 48.0F, 0.035F, 0.055F, 0.080F, 0.78F);
        draw_panel_outline(20.0F, 20.0F, 420.0F, 48.0F, 0.30F, 0.48F, 0.70F, 0.72F);
        draw_ui_text(34.0F, 35.0F, 1.9F, scene.objective_text, UiTextStyle::Section);
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

}  // namespace ae::render
