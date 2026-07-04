#include "ae/core/log.h"
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
    float gap = 8.0f + state.crosshair_spread * 20.0f;
    float len = 12.0f;
    float thickness = 2.0f;
    ImU32 color = IM_COL32(255, 255, 255, 200);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddLine({cx - gap - len, cy}, {cx - gap, cy}, color, thickness);
    dl->AddLine({cx + gap, cy}, {cx + gap + len, cy}, color, thickness);
    dl->AddLine({cx, cy - gap - len}, {cx, cy - gap}, color, thickness);
    dl->AddLine({cx, cy + gap}, {cx, cy + gap + len}, color, thickness);
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
            r.register_type("grenade",      [] { return std::make_unique<HudAbilityIcon>(HudAbilityIcon::Grenade); });
            r.register_type("special",      [] { return std::make_unique<HudAbilityIcon>(HudAbilityIcon::Special); });
            r.register_type("artifact",     [] { return std::make_unique<HudAbilityIcon>(HudAbilityIcon::Artifact); });
            r.register_type("ultimate",     [] { return std::make_unique<HudAbilityIcon>(HudAbilityIcon::Ultimate); });
        }
    } auto_register;
}  // namespace

}  // namespace ae::ui
