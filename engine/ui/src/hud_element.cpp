#include "ae/core/log.h"
#include "ae/ui/ahamkara_ui.h"
#include "ae/ui/hud_element.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>

#include "imgui.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


#define AE_LOG_CATEGORY "UI"

namespace ae::ui {

namespace {

constexpr ImU32 kHudPanelTop = IM_COL32(12, 20, 30, 214);
constexpr ImU32 kHudPanelBottom = IM_COL32(5, 10, 17, 232);
constexpr ImU32 kHudLine = IM_COL32(117, 151, 164, 104);
constexpr ImU32 kHudText = IM_COL32(240, 244, 245, 248);
constexpr ImU32 kHudMuted = IM_COL32(154, 169, 174, 220);
constexpr ImU32 kHudDim = IM_COL32(92, 108, 115, 190);
constexpr ImU32 kHudAmber = IM_COL32(244, 169, 60, 250);
constexpr ImU32 kHudCyan = IM_COL32(92, 196, 211, 245);
constexpr ImU32 kHudDanger = IM_COL32(238, 75, 65, 250);

void draw_text(ImDrawList* dl, ImFont* font, float size, ImVec2 position,
               ImU32 color, const char* text) {
    if (font != nullptr) {
        dl->AddText(font, size, position, color, text);
    } else {
        dl->AddText(position, color, text);
    }
}

void draw_ability_glyph(ImDrawList* dl, HudAbilityIcon::AbilityType type,
                        ImVec2 center, float radius, ImU32 color) {
    const float thickness = 1.7F;
    switch (type) {
    case HudAbilityIcon::Grenade:
        dl->AddCircle(center, radius * 0.62F, color, 20, thickness);
        dl->AddLine({center.x - radius * 0.72F, center.y},
                    {center.x + radius * 0.72F, center.y}, color, thickness);
        dl->AddLine({center.x, center.y - radius * 0.72F},
                    {center.x, center.y + radius * 0.72F}, color, thickness);
        break;
    case HudAbilityIcon::Special:
        dl->PathLineTo({center.x + radius * 0.10F, center.y - radius * 0.82F});
        dl->PathLineTo({center.x - radius * 0.48F, center.y + radius * 0.02F});
        dl->PathLineTo({center.x - radius * 0.02F, center.y + radius * 0.02F});
        dl->PathLineTo({center.x - radius * 0.16F, center.y + radius * 0.82F});
        dl->PathLineTo({center.x + radius * 0.52F, center.y - radius * 0.14F});
        dl->PathLineTo({center.x + radius * 0.08F, center.y - radius * 0.14F});
        dl->PathStroke(color, ImDrawFlags_Closed, thickness);
        break;
    case HudAbilityIcon::Artifact:
        dl->AddCircle(center, radius * 0.72F, color, 24, thickness);
        dl->AddCircle(center, radius * 0.32F, color, 16, thickness);
        dl->AddCircleFilled(center, radius * 0.10F, color, 12);
        break;
    case HudAbilityIcon::Ultimate:
        dl->AddQuad({center.x, center.y - radius * 0.82F},
                    {center.x + radius * 0.82F, center.y},
                    {center.x, center.y + radius * 0.82F},
                    {center.x - radius * 0.82F, center.y}, color, thickness);
        dl->AddCircleFilled(center, radius * 0.22F, color, 16);
        break;
    }
}

void draw_weapon_silhouette(ImDrawList* dl, ImVec2 origin, float scale, ImU32 color) {
    const float x = origin.x;
    const float y = origin.y;
    dl->AddRectFilled({x, y + 8.0F * scale},
                      {x + 62.0F * scale, y + 17.0F * scale}, color, 1.0F);
    dl->AddTriangleFilled({x + 10.0F * scale, y + 17.0F * scale},
                          {x + 26.0F * scale, y + 17.0F * scale},
                          {x + 19.0F * scale, y + 29.0F * scale}, color);
    dl->AddRectFilled({x + 60.0F * scale, y + 10.0F * scale},
                      {x + 88.0F * scale, y + 14.0F * scale}, color);
    dl->AddRectFilled({x - 13.0F * scale, y + 10.0F * scale},
                      {x + 3.0F * scale, y + 17.0F * scale}, color);
    dl->AddRectFilled({x + 31.0F * scale, y + 2.0F * scale},
                      {x + 49.0F * scale, y + 7.0F * scale}, color, 1.0F);
}

} // namespace

// ── HudElement base ───────────────────────────────────────────────────────────

void HudElement::set_layout(float x, float y, const std::string& anchor, float w, float h) {
    layout_x_ = x;
    layout_y_ = y;
    anchor_ = anchor;
    layout_w_ = w;
    layout_h_ = h;
}

void HudElement::compute_position(float sw, float sh, float el_w, float el_h,
                                   float& out_x, float& out_y) const {
    float scale = ImGui::GetIO().DisplayFramebufferScale.x > 0
                      ? ImGui::GetIO().DisplayFramebufferScale.x : 1.0f;
    float ax = layout_x_ * sw;
    float ay = layout_y_ * sh;
    float pw = (layout_w_ > 0 ? layout_w_ : el_w) * scale;
    float ph = (layout_h_ > 0 ? layout_h_ : el_h) * scale;

    if (anchor_ == "top_left")        { out_x = ax;           out_y = ay; }
    else if (anchor_ == "top_center") { out_x = ax - pw * 0.5f; out_y = ay; }
    else if (anchor_ == "top_right")  { out_x = ax - pw;      out_y = ay; }
    else if (anchor_ == "center")     { out_x = ax - pw * 0.5f; out_y = ay - ph * 0.5f; }
    else if (anchor_ == "bottom_left") { out_x = ax;          out_y = ay - ph; }
    else if (anchor_ == "bottom_center") { out_x = ax - pw * 0.5f; out_y = ay - ph; }
    else if (anchor_ == "bottom_right") { out_x = ax - pw;     out_y = ay - ph; }
    else { out_x = ax; out_y = ay; }
}

// ── Health Bar ────────────────────────────────────────────────────────────────

void HudHealthBar::render(float sw, float sh, const HudState& state) {
    if (!visible_) return;
    float bar_w = 320.0f, bar_h = 28.0f;
    float x, y;
    compute_position(sw, sh, bar_w, bar_h, x, y);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    float scale = ImGui::GetIO().DisplayFramebufferScale.x > 0
                      ? ImGui::GetIO().DisplayFramebufferScale.x : 1.0f;
    bar_w *= scale; bar_h *= scale;

    // Background
    dl->AddRectFilled({x, y}, {x + bar_w, y + bar_h}, IM_COL32(10, 10, 15, 200), 6.0f * scale);
    dl->AddRect({x - 1, y - 1}, {x + bar_w + 1, y + bar_h + 1}, IM_COL32(60, 60, 80, 160), 6.0f * scale, 0, 1.5f * scale);

    // Shield bar (overlays top of health if present)
    if (state.shield > 0.0f && state.max_shield > 0.0f) {
        float shield_pct = std::clamp(state.shield / state.max_shield, 0.0f, 1.0f);
        dl->AddRectFilled({x, y}, {x + bar_w * shield_pct, y + bar_h * 0.45f},
                          IM_COL32(80, 140, 220, 220), 4.0f * scale);
    }

    // Health fill
    float pct = std::clamp(state.health / state.max_health, 0.0f, 1.0f);
    ImU32 color = IM_COL32(200, 50, 50, 230);
    if (pct > 0.6f)      color = IM_COL32(60, 180, 80, 230);
    else if (pct > 0.3f) color = IM_COL32(230, 180, 40, 230);
    else if (pct < 0.15f) {
        static float pulse = 0.0f;
        pulse += 0.05f;
        float blink = 0.7f + 0.3f * std::sin(pulse * 3.0f);
        color = IM_COL32(220, 40, 40, (int)(230 * blink));
    }
    dl->AddRectFilled({x, y + (state.shield > 0 ? bar_h * 0.45f : 0)},
                      {x + bar_w * pct, y + bar_h}, color, 4.0f * scale);

    // Text
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.0f / %.0f", state.health, state.max_health);
    ImVec2 ts = ImGui::CalcTextSize(buf);
    dl->AddText({x + (bar_w - ts.x) * 0.5f, y + (bar_h - ts.y) * 0.5f},
                IM_COL32(255, 255, 255, 240), buf);
}

// ── Ammo Counter ──────────────────────────────────────────────────────────────

void HudAmmoCounter::render(float sw, float sh, const HudState& state) {
    if (!visible_) return;
    float x, y;
    compute_position(sw, sh, 180, 60, x, y);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    float scale = ImGui::GetIO().DisplayFramebufferScale.x > 0
                      ? ImGui::GetIO().DisplayFramebufferScale.x : 1.0f;

    // Background panel
    float pw = 180 * scale, ph = 60 * scale;
    dl->AddRectFilled({x - pw, y - ph},
                      {x, y}, IM_COL32(8, 8, 14, 210), 6.0f * scale);

    // Weapon name
    dl->AddText({x - pw + 10 * scale, y - ph + 4 * scale},
                IM_COL32(180, 180, 200, 220), state.weapon_name);

    // Ammo count (large)
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%d", state.ammo_current);
    std::string ammo_str(buf);
    ImVec2 big_size = ImGui::CalcTextSize(buf);
    float font_scale_large = 1.3f;
    dl->AddText(nullptr, ImGui::GetFontSize() * font_scale_large,
                {x - pw + 10 * scale, y - ph + 18 * scale},
                IM_COL32(255, 255, 255, 255), buf);

    // Reserve ammo
    std::snprintf(buf, sizeof(buf), "  / %d  [%d]", state.ammo_max, state.reserve_ammo);
    dl->AddText({x - pw + 10 * scale + big_size.x * font_scale_large, y - ph + 20 * scale},
                IM_COL32(160, 165, 180, 200), buf);

    // Weapon box icon (simple colored rectangle representing weapon type)
    ImU32 wpn_color = IM_COL32(200, 160, 60, 200);
    if (state.weapon_index == 1) wpn_color = IM_COL32(60, 140, 200, 200);
    else if (state.weapon_index == 2) wpn_color = IM_COL32(200, 80, 60, 200);
    dl->AddRectFilled({x - 44 * scale, y - ph + 8 * scale},
                      {x - 12 * scale, y - 8 * scale}, wpn_color, 4.0f * scale);
}

// ── Ability Icon ──────────────────────────────────────────────────────────────

const char* HudAbilityIcon::label_for(AbilityType t) {
    switch (t) {
        case Grenade:  return "G";
        case Special:  return "S";
        case Artifact: return "A";
        case Ultimate: return "U";
    }
    return "?";
}

ImVec4 HudAbilityIcon::color_for(AbilityType t) {
    switch (t) {
        case Grenade:  return {0.85f, 0.50f, 0.15f, 1.0f};
        case Special:  return {0.20f, 0.60f, 0.90f, 1.0f};
        case Artifact: return {0.70f, 0.30f, 0.80f, 1.0f};
        case Ultimate: return {0.95f, 0.85f, 0.15f, 1.0f};
    }
    return {1, 1, 1, 1};
}

float HudAbilityIcon::cooldown_for(const HudState& s, AbilityType t) const {
    switch (t) {
        case Grenade:  return s.grenade_cooldown;
        case Special:  return s.special_cooldown;
        case Artifact: return s.artifact_cooldown;
        case Ultimate: return 1.0f - s.ultimate_charge;
    }
    return 0;
}

bool HudAbilityIcon::available_for(const HudState& s, AbilityType t) const {
    switch (t) {
        case Grenade:  return s.grenade_available;
        case Special:  return s.special_available;
        case Artifact: return s.artifact_available;
        case Ultimate: return s.ultimate_ready;
    }
    return false;
}

int HudAbilityIcon::count_for(const HudState& s, AbilityType t) const {
    switch (t) {
        case Grenade:  return s.grenade_count;
        case Special:  return 0;
        case Artifact: return 0;
        case Ultimate: return 0;
    }
    return 0;
}

void HudAbilityIcon::render(float sw, float sh, const HudState& state) {
    if (!visible_) return;
    float sz = 56.0f;
    float x, y;
    compute_position(sw, sh, sz, sz, x, y);

    float scale = ImGui::GetIO().DisplayFramebufferScale.x > 0
                      ? ImGui::GetIO().DisplayFramebufferScale.x : 1.0f;
    sz *= scale;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec4 col = color_for(type_);
    ImU32 bg = IM_COL32(12, 12, 20, 210);
    ImU32 fg = IM_COL32((int)(col.x * 255), (int)(col.y * 255), (int)(col.z * 255), 240);

    float cd = cooldown_for(state, type_);
    bool available = available_for(state, type_);

    // Background square
    dl->AddRectFilled({x, y}, {x + sz, y + sz}, bg, 6.0f * scale);

    // Cooldown overlay (clockwise sweep)
    if (cd > 0.01f && !available) {
        float angle = cd * 2.0f * (float)M_PI;
        dl->PathArcTo({x + sz * 0.5f, y + sz * 0.5f}, sz * 0.55f, -(float)M_PI * 0.5f, -(float)M_PI * 0.5f + angle, 32);
        dl->PathStroke(IM_COL32(0, 0, 0, 180), 0, sz * 0.5f);
    }

    // Letter
    const char* lbl = label_for(type_);
    ImVec2 ts = ImGui::CalcTextSize(lbl);
    float font_scale_icon = 1.6f;
    dl->AddText(nullptr, ImGui::GetFontSize() * font_scale_icon,
                {x + (sz - ts.x * font_scale_icon) * 0.5f, y + (sz - ts.y * font_scale_icon) * 0.5f},
                available ? fg : IM_COL32(80, 80, 90, 180), lbl);

    // Border
    ImU32 border = available ? IM_COL32((int)(col.x * 200), (int)(col.y * 200), (int)(col.z * 200), 200)
                             : IM_COL32(40, 40, 50, 150);
    dl->AddRect({x, y}, {x + sz, y + sz}, border, 6.0f * scale, 0, 2.0f * scale);

    // Count (grenades)
    if (count_for(state, type_) > 0) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%d", count_for(state, type_));
        dl->AddText({x + sz - 16 * scale, y + sz - 18 * scale},
                    IM_COL32(255, 255, 255, 240), buf);
    }

    // Keybind hint below
    const char* key = (type_ == Grenade) ? "Q" : (type_ == Special) ? "E" : (type_ == Artifact) ? "C" : "F";
    ImVec2 kts = ImGui::CalcTextSize(key);
    dl->AddText({x + (sz - kts.x) * 0.5f, y + sz + 4},
                IM_COL32(130, 135, 150, 180), key);
}

// ── Crosshair ─────────────────────────────────────────────────────────────────

void HudCrosshair::render(float sw, float sh, const HudState& state) {
    if (!visible_ || !state.crosshair_visible) return;
    float cx = sw * 0.5f, cy = sh * 0.5f;
    const float spread = std::clamp(state.crosshair_spread, 0.0F, 1.0F);
    const float gap = 5.0F + spread * 8.0F;
    const float len = 7.0F;
    const ImU32 outline = IM_COL32(0, 0, 0, 180);
    const ImU32 color = IM_COL32(235, 242, 243, 225);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const auto arm = [&](ImVec2 a, ImVec2 b) {
        dl->AddLine(a, b, outline, 3.0F);
        dl->AddLine(a, b, color, 1.25F);
    };
    arm({cx - gap - len, cy}, {cx - gap, cy});
    arm({cx + gap, cy}, {cx + gap + len, cy});
    arm({cx, cy - gap - len}, {cx, cy - gap});
    arm({cx, cy + gap}, {cx, cy + gap + len});
    dl->AddCircleFilled({cx, cy}, 1.7F, outline, 12);
    dl->AddCircleFilled({cx, cy}, 0.85F, kHudCyan, 12);
}

// ── Consolidated Combat Cluster ──────────────────────────────────────────────

void HudCombatCluster::render(float sw, float sh, const HudState& state) {
    if (!visible_)
        return;

    const float responsive = std::clamp(sw / 1600.0F, 0.78F, 1.0F);
    const float panel_w = std::min(560.0F * responsive, sw - 48.0F);
    const float panel_h = 132.0F * responsive;
    const float x = std::max(24.0F, layout_x_ * sw);
    const float bottom = std::min(sh - 24.0F, layout_y_ * sh);
    const float y = bottom - panel_h;
    const float u = responsive;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* small_font = menu_font_for_role("small");
    ImFont* body_font = menu_font_for_role("body");
    ImFont* header_font = menu_font_for_role("header");

    // A restrained glass panel with a short angled tail. It remains legible
    // over bright terrain without becoming an opaque rectangular dashboard.
    dl->AddRectFilledMultiColor(
        {x, y + 12.0F * u}, {x + panel_w, y + panel_h},
        kHudPanelTop, kHudPanelTop, kHudPanelBottom, kHudPanelBottom);
    dl->AddTriangleFilled(
        {x + panel_w, y + 12.0F * u},
        {x + panel_w + 18.0F * u, y + 12.0F * u},
        {x + panel_w, y + 32.0F * u}, kHudPanelTop);
    dl->AddLine({x, y + panel_h}, {x + panel_w, y + panel_h}, kHudLine, 1.0F);

    const float health_pct = state.max_health > 0.0F
                                 ? std::clamp(state.health / state.max_health, 0.0F, 1.0F)
                                 : 0.0F;
    const bool critical_health = health_pct <= 0.25F;
    const ImU32 health_color = critical_health ? kHudDanger : kHudAmber;
    const float health_y = y + 2.0F * u;
    const float health_h = 7.0F * u;
    dl->AddRectFilled({x, health_y}, {x + panel_w, health_y + health_h},
                      IM_COL32(27, 38, 46, 215));
    dl->AddRectFilled({x, health_y},
                      {x + panel_w * health_pct, health_y + health_h}, health_color);

    if (state.max_shield > 0.0F && state.shield > 0.0F) {
        const float shield_pct = std::clamp(state.shield / state.max_shield, 0.0F, 1.0F);
        dl->AddRectFilled({x, health_y - 3.0F * u},
                          {x + panel_w * shield_pct, health_y - 1.0F * u}, kHudCyan);
    }

    char value[32] {};
    std::snprintf(value, sizeof(value), "%.0f", state.health);
    draw_text(dl, small_font, 12.0F * u, {x, y - 14.0F * u},
              critical_health ? kHudDanger : kHudMuted, "VITALS");
    draw_text(dl, small_font, 12.0F * u, {x + 47.0F * u, y - 14.0F * u},
              kHudText, value);

    const float content_y = y + 24.0F * u;
    const float ability_size = 43.0F * u;
    const float ability_gap = 5.0F * u;
    const HudAbilityIcon::AbilityType ability_types[] = {
        HudAbilityIcon::Grenade, HudAbilityIcon::Special,
        HudAbilityIcon::Artifact, HudAbilityIcon::Ultimate};
    const char* keys[] = {"Q", "E", "C", "F"};
    const float cooldowns[] = {
        state.grenade_cooldown, state.special_cooldown,
        state.artifact_cooldown, 1.0F - state.ultimate_charge};
    const bool available[] = {
        state.grenade_available, state.special_available,
        state.artifact_available, state.ultimate_ready};

    for (int i = 0; i < 4; ++i) {
        const float ax = x + i * (ability_size + ability_gap);
        const float cd = std::clamp(cooldowns[i], 0.0F, 1.0F);
        const ImU32 tint = available[i] ? (i == 3 ? kHudAmber : kHudCyan) : kHudDim;
        dl->AddRectFilled({ax, content_y}, {ax + ability_size, content_y + ability_size},
                          IM_COL32(18, 31, 43, 218), 2.0F * u);
        if (!available[i] && cd > 0.0F) {
            dl->AddRectFilled({ax, content_y + ability_size * (1.0F - cd)},
                              {ax + ability_size, content_y + ability_size},
                              IM_COL32(4, 8, 13, 180), 2.0F * u);
        }
        dl->AddRect({ax, content_y}, {ax + ability_size, content_y + ability_size},
                    tint, 2.0F * u, 0, available[i] ? 1.4F : 1.0F);
        draw_ability_glyph(dl, ability_types[i],
                           {ax + ability_size * 0.5F, content_y + ability_size * 0.48F},
                           ability_size * 0.25F, tint);
        draw_text(dl, small_font, 10.0F * u,
                  {ax + 4.0F * u, content_y + ability_size - 13.0F * u}, kHudMuted, keys[i]);

        if (i == 0 && state.grenade_count > 0) {
            std::snprintf(value, sizeof(value), "%d", state.grenade_count);
            draw_text(dl, small_font, 10.0F * u,
                      {ax + ability_size - 12.0F * u, content_y + 3.0F * u}, kHudText, value);
        }
    }

    const float divider_x = x + 207.0F * u;
    dl->AddLine({divider_x, content_y}, {divider_x, y + panel_h - 14.0F * u},
                kHudLine, 1.0F);

    const float weapon_x = divider_x + 16.0F * u;
    draw_text(dl, small_font, 11.0F * u, {weapon_x, content_y - 1.0F * u},
              kHudMuted, "PRIMARY // AUTO");
    draw_weapon_silhouette(dl, {weapon_x + 8.0F * u, content_y + 27.0F * u},
                           0.72F * u, kHudText);
    draw_text(dl, small_font, 11.0F * u,
              {weapon_x, y + panel_h - 24.0F * u}, kHudMuted, state.weapon_name);

    const float ammo_x = x + panel_w - 128.0F * u;
    dl->AddLine({ammo_x - 13.0F * u, content_y},
                {ammo_x - 13.0F * u, y + panel_h - 14.0F * u}, kHudLine, 1.0F);
    std::snprintf(value, sizeof(value), "%d", state.ammo_current);
    const ImU32 ammo_color = state.ammo_current <= 5 ? kHudDanger : kHudText;
    draw_text(dl, header_font, 38.0F * u, {ammo_x, content_y - 7.0F * u},
              ammo_color, value);
    std::snprintf(value, sizeof(value), "/ %d", state.reserve_ammo);
    draw_text(dl, body_font, 16.0F * u,
              {ammo_x + 4.0F * u, content_y + 42.0F * u}, kHudMuted, value);

    const float mag_pct = state.ammo_max > 0
                              ? std::clamp(static_cast<float>(state.ammo_current) /
                                               static_cast<float>(state.ammo_max),
                                           0.0F, 1.0F)
                              : 0.0F;
    const float mag_y = y + panel_h - 18.0F * u;
    const float mag_w = 98.0F * u;
    dl->AddRectFilled({ammo_x, mag_y}, {ammo_x + mag_w, mag_y + 3.0F * u},
                      IM_COL32(43, 55, 62, 220));
    dl->AddRectFilled({ammo_x, mag_y},
                      {ammo_x + mag_w * mag_pct, mag_y + 3.0F * u},
                      state.ammo_current <= 5 ? kHudDanger : kHudAmber);
}

// ── Field Optic HUD ──────────────────────────────────────────────────────────

void HudFieldFrame::render(float sw, float sh, const HudState& state) {
    if (!visible_)
        return;

    constexpr float kSectionScale = 1.30F;
    const float u = std::clamp(sw / 1920.0F, 0.74F, 1.12F) * kSectionScale;
    const float safe = std::max(22.0F, 30.0F * u);
    const float baseline = sh - safe;
    const float left = safe;
    const float right = sw - safe;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* small_font = menu_font_for_role("small");
    ImFont* body_font = menu_font_for_role("body");
    ImFont* header_font = menu_font_for_role("header");

    const float health_pct = state.max_health > 0.0F
                                 ? std::clamp(state.health / state.max_health, 0.0F, 1.0F)
                                 : 0.0F;
    const float shield_pct = state.max_shield > 0.0F
                                 ? std::clamp(state.shield / state.max_shield, 0.0F, 1.0F)
                                 : 0.0F;
    const bool critical = health_pct <= 0.25F;
    const ImU32 vital_color = critical ? kHudDanger : kHudAmber;
    char value[32] {};

    // Left: survival strip. The angled cap and segmented bar read as an
    // instrument instead of an opaque dashboard over the game world.
    const float vital_w = 236.0F * u;
    const float vital_y = baseline - 92.0F * u;
    dl->AddRectFilledMultiColor(
        {left, vital_y}, {left + vital_w, baseline - 36.0F * u},
        IM_COL32(5, 11, 17, 112), IM_COL32(5, 11, 17, 164),
        IM_COL32(5, 11, 17, 184), IM_COL32(5, 11, 17, 122));
    dl->AddLine({left, vital_y}, {left + vital_w - 18.0F * u, vital_y}, kHudLine, 1.0F * u);
    dl->AddLine({left, vital_y}, {left, baseline - 36.0F * u}, kHudLine, 1.0F * u);
    dl->AddLine({left, baseline - 36.0F * u}, {left + vital_w, baseline - 36.0F * u}, kHudLine, 1.0F * u);
    dl->AddLine({left + vital_w - 18.0F * u, vital_y}, {left + vital_w, vital_y + 18.0F * u}, kHudLine, 1.0F * u);

    draw_text(dl, small_font, 10.0F * u, {left + 12.0F * u, vital_y + 10.0F * u}, kHudMuted, "OPERATIVE // VITALS");
    std::snprintf(value, sizeof(value), "%03.0f", std::max(state.health, 0.0F));
    draw_text(dl, header_font, 31.0F * u, {left + 11.0F * u, vital_y + 23.0F * u}, vital_color, value);
    draw_text(dl, small_font, 11.0F * u, {left + 86.0F * u, vital_y + 43.0F * u}, kHudMuted, "HEALTH");

    const float bar_x = left + 12.0F * u;
    const float health_y = baseline - 29.0F * u;
    const float bar_w = vital_w - 24.0F * u;
    constexpr int kHealthSegments = 12;
    for (int i = 0; i < kHealthSegments; ++i) {
        const float segment_x = bar_x + (bar_w / kHealthSegments) * static_cast<float>(i);
        const float segment_w = bar_w / kHealthSegments - 2.0F * u;
        const float threshold = static_cast<float>(i + 1) / static_cast<float>(kHealthSegments);
        dl->AddRectFilled({segment_x, health_y}, {segment_x + segment_w, health_y + 4.0F * u},
                          threshold <= health_pct ? vital_color : IM_COL32(35, 48, 54, 190));
    }
    if (shield_pct > 0.0F) {
        dl->AddRectFilled({bar_x, health_y - 7.0F * u},
                          {bar_x + bar_w * shield_pct, health_y - 5.0F * u}, kHudCyan);
    }

    // The ability rail uses distinct geometric glyphs plus key labels; color
    // reinforces readiness but is never the only state cue.
    const float ability_y = baseline - 25.0F * u;
    const float ability_x = left + vital_w + 14.0F * u;
    const float ability_size = 32.0F * u;
    const HudAbilityIcon::AbilityType ability_types[] = {
        HudAbilityIcon::Grenade, HudAbilityIcon::Special,
        HudAbilityIcon::Artifact, HudAbilityIcon::Ultimate};
    const char* keys[] = {"Q", "E", "C", "F"};
    const float cooldowns[] = {
        state.grenade_cooldown, state.special_cooldown,
        state.artifact_cooldown, 1.0F - state.ultimate_charge};
    const bool available[] = {
        state.grenade_available, state.special_available,
        state.artifact_available, state.ultimate_ready};
    for (int i = 0; i < 4; ++i) {
        const float ax = ability_x + static_cast<float>(i) * (ability_size + 7.0F * u);
        const float cooldown = std::clamp(cooldowns[i], 0.0F, 1.0F);
        const ImU32 tint = available[i] ? (i == 3 ? kHudAmber : kHudCyan) : kHudDim;
        dl->AddRectFilled({ax, ability_y - ability_size}, {ax + ability_size, ability_y},
                          IM_COL32(7, 16, 24, 194), 1.0F * u);
        dl->AddRect({ax, ability_y - ability_size}, {ax + ability_size, ability_y}, tint, 1.0F * u);
        if (!available[i] && cooldown > 0.0F) {
            dl->AddRectFilled({ax, ability_y - ability_size},
                              {ax + ability_size, ability_y - ability_size * (1.0F - cooldown)},
                              IM_COL32(2, 6, 10, 156));
        }
        draw_ability_glyph(dl, ability_types[i],
                           {ax + ability_size * 0.5F, ability_y - ability_size * 0.57F},
                           ability_size * 0.28F, tint);
        draw_text(dl, small_font, 9.0F * u, {ax + 3.0F * u, ability_y - 12.0F * u}, kHudMuted, keys[i]);
    }

    // Right: weapon telemetry. The magazine count is the dominant cue; labels
    // and the segmented meter remain secondary so the view stays readable.
    const float weapon_w = 248.0F * u;
    const float weapon_x = right - weapon_w;
    const float weapon_y = baseline - 92.0F * u;
    dl->AddRectFilledMultiColor(
        {weapon_x, weapon_y}, {right, baseline - 36.0F * u},
        IM_COL32(5, 11, 17, 164), IM_COL32(5, 11, 17, 112),
        IM_COL32(5, 11, 17, 122), IM_COL32(5, 11, 17, 184));
    dl->AddLine({weapon_x + 18.0F * u, weapon_y}, {right, weapon_y}, kHudLine, 1.0F * u);
    dl->AddLine({right, weapon_y}, {right, baseline - 36.0F * u}, kHudLine, 1.0F * u);
    dl->AddLine({weapon_x, baseline - 36.0F * u}, {right, baseline - 36.0F * u}, kHudLine, 1.0F * u);
    dl->AddLine({weapon_x + 18.0F * u, weapon_y}, {weapon_x, weapon_y + 18.0F * u}, kHudLine, 1.0F * u);
    draw_text(dl, small_font, 10.0F * u, {weapon_x + 12.0F * u, weapon_y + 10.0F * u},
              kHudMuted, state.is_reloading ? "WEAPON // RELOADING" : "WEAPON // PRIMARY");
    draw_weapon_silhouette(dl, {weapon_x + 13.0F * u, weapon_y + 32.0F * u}, 0.60F * u, kHudText);
    draw_text(dl, small_font, 11.0F * u, {weapon_x + 12.0F * u, baseline - 52.0F * u}, kHudMuted, state.weapon_name);

    const float ammo_x = right - 94.0F * u;
    const ImU32 ammo_color = state.ammo_current <= 5 ? kHudDanger : kHudText;
    std::snprintf(value, sizeof(value), "%02d", std::max(0, state.ammo_current));
    draw_text(dl, header_font, 40.0F * u, {ammo_x, weapon_y + 23.0F * u}, ammo_color, value);
    std::snprintf(value, sizeof(value), "/ %03d", std::max(0, state.reserve_ammo));
    draw_text(dl, body_font, 15.0F * u, {ammo_x + 4.0F * u, weapon_y + 62.0F * u}, kHudMuted, value);

    const float magazine_pct = state.ammo_max > 0
                                   ? std::clamp(static_cast<float>(state.ammo_current) / static_cast<float>(state.ammo_max), 0.0F, 1.0F)
                                   : 0.0F;
    const float mag_x = weapon_x + 12.0F * u;
    const float mag_y = baseline - 28.0F * u;
    const float mag_w = weapon_w - 24.0F * u;
    constexpr int kMagazineSegments = 10;
    for (int i = 0; i < kMagazineSegments; ++i) {
        const float segment_x = mag_x + (mag_w / kMagazineSegments) * static_cast<float>(i);
        const float segment_w = mag_w / kMagazineSegments - 2.0F * u;
        const float threshold = static_cast<float>(i + 1) / static_cast<float>(kMagazineSegments);
        dl->AddRectFilled({segment_x, mag_y}, {segment_x + segment_w, mag_y + 3.0F * u},
                          threshold <= magazine_pct ? kHudAmber : IM_COL32(35, 48, 54, 190));
    }
}

// ── Factory ───────────────────────────────────────────────────────────────────

HudElementRegistry& HudElementRegistry::instance() {
    static HudElementRegistry reg;
    return reg;
}

void HudElementRegistry::register_type(const std::string& name, HudElementFactory factory) {
    factories_[name] = std::move(factory);
}

std::unique_ptr<HudElement> HudElementRegistry::create(const std::string& type) const {
    auto it = factories_.find(type);
    if (it != factories_.end()) return it->second();
    return nullptr;
}

// ── Static registrations ──────────────────────────────────────────────────────

namespace {
    struct AutoRegister {
        AutoRegister() {
            auto& r = HudElementRegistry::instance();
            r.register_type("health_bar",   [] { return std::make_unique<HudHealthBar>(); });
            r.register_type("ammo_counter", [] { return std::make_unique<HudAmmoCounter>(); });
            r.register_type("crosshair",    [] { return std::make_unique<HudCrosshair>(); });
            r.register_type("combat_cluster", [] { return std::make_unique<HudCombatCluster>(); });
            r.register_type("field_frame", [] { return std::make_unique<HudFieldFrame>(); });
            r.register_type("grenade",      [] { return std::make_unique<HudAbilityIcon>(HudAbilityIcon::Grenade); });
            r.register_type("special",      [] { return std::make_unique<HudAbilityIcon>(HudAbilityIcon::Special); });
            r.register_type("artifact",     [] { return std::make_unique<HudAbilityIcon>(HudAbilityIcon::Artifact); });
            r.register_type("ultimate",     [] { return std::make_unique<HudAbilityIcon>(HudAbilityIcon::Ultimate); });
        }
    } auto_register;
}  // namespace

}  // namespace ae::ui
