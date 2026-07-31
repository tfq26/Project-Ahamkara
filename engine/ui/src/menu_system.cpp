#include "ae/ui/menu_system.h"

#include "ae/core/log.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include "imgui.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define AE_LOG_CATEGORY "UI"

namespace ae::ui {

namespace {

float hex_to_float(const std::string& s) {
    try { return std::stof(s); } catch (...) { return 0.0f; }
}

ImVec4 parse_color(const std::vector<float>& c) {
    if (c.size() >= 4) return {c[0], c[1], c[2], c[3]};
    if (c.size() >= 3) return {c[0], c[1], c[2], 1.0f};
    return {1, 1, 1, 1};
}

float get_theme_float(const std::unordered_map<std::string, float>& map, const std::string& key, float def = 1.0f) {
    auto it = map.find(key);
    return it != map.end() ? it->second : def;
}

ImVec4 get_theme_color(const std::unordered_map<std::string, ImVec4>& map, const std::string& key, const ImVec4& def = {1,1,1,1}) {
    auto it = map.find(key);
    return it != map.end() ? it->second : def;
}

// Minimal JSON parser for our menu format.
class MiniJson {
public:
    MiniJson(const std::string& src) : s_(src), p_(0) {}
    void skip_ws() { while (p_ < s_.size() && (std::isspace(s_[p_]) || s_[p_] == ',')) ++p_; }
    char peek() { skip_ws(); return p_ < s_.size() ? s_[p_] : 0; }
    char next() { skip_ws(); return p_ < s_.size() ? s_[p_++] : 0; }

    std::string read_string() {
        if (next() != '"') return {};
        std::string r;
        while (p_ < s_.size() && s_[p_] != '"') { if (s_[p_] == '\\') { ++p_; if (p_ < s_.size()) r += s_[p_++]; } else r += s_[p_++]; }
        ++p_; return r;
    }

    float read_number() {
        skip_ws();
        std::size_t start = p_;
        if (p_ < s_.size() && s_[p_] == '-') ++p_;
        while (p_ < s_.size() && (std::isdigit(s_[p_]) || s_[p_] == '.')) ++p_;
        return hex_to_float(s_.substr(start, p_ - start));
    }

    bool read_bool() {
        skip_ws();
        if (p_ + 4 <= s_.size() && s_.substr(p_, 4) == "true") { p_ += 4; return true; }
        if (p_ + 5 <= s_.size() && s_.substr(p_, 5) == "false") { p_ += 5; return false; }
        return false;
    }

    std::vector<float> read_float_array() {
        std::vector<float> v;
        if (next() != '[') return v;
        while (peek() != ']') v.push_back(read_number());
        next(); return v;
    }

    void skip_value() {
        skip_ws(); if (p_ >= s_.size()) return;
        char c = s_[p_];
        if (c == '{') { ++p_; int d = 1; while (d > 0 && p_ < s_.size()) { if (s_[p_]=='{') ++d; else if (s_[p_]=='}') --d; ++p_; } }
        else if (c == '[') { ++p_; int d = 1; while (d > 0 && p_ < s_.size()) { if (s_[p_]=='[') ++d; else if (s_[p_]==']') --d; ++p_; } }
        else if (c == '"') { read_string(); }
        else { while (p_ < s_.size() && !std::isspace(s_[p_]) && s_[p_] != ',' && s_[p_] != '}' && s_[p_] != ']') ++p_; }
    }

    const std::string& s_;
    std::size_t p_;
};

// Forward
static void parse_element(MiniJson& j, MenuSystem::ParsedElement& el);
static void parse_screen_body(MiniJson& j, MenuSystem::ParsedScreen& screen);

void parse_element(MiniJson& j, MenuSystem::ParsedElement& el) {
    el.type = "panel"; // default
    while (j.peek() != '}') {
        std::string key = j.read_string();
        if (j.next() != ':') break;
        if (key == "type") el.type = j.read_string();
        else if (key == "content") el.content = j.read_string();
        else if (key == "label") el.label = j.read_string();
        else if (key == "action") el.action = j.read_string();
        else if (key == "style") el.style = j.read_string();
        else if (key == "color") el.color = j.read_string();
        else if (key == "font") el.font = j.read_string();
        else if (key == "setting") el.setting = j.read_string();
        else if (key == "map_id") el.map_id = j.read_string();
        else if (key == "description") el.description = j.read_string();
        else if (key == "players") el.players = j.read_string();
        else if (key == "id") el.id = j.read_string();
        else if (key == "language") el.language = j.read_string();
        else if (key == "anchor") el.anchor = j.read_string();
        else if (key == "x") el.x = j.read_number();
        else if (key == "y") el.y = j.read_number();
        else if (key == "width") el.width = j.read_number();
        else if (key == "height") el.height = j.read_number();
        else if (key == "min") el.min = j.read_number();
        else if (key == "max") el.max = j.read_number();
        else if (key == "radius") el.radius = j.read_number();
        else if (key == "thickness") el.thickness = j.read_number();
        else if (key == "rounding") el.rounding = j.read_number();
        else if (key == "background") { auto v = j.read_float_array(); if (v.size() >= 4) {} }
        else if (key == "overlay") el.content = j.read_bool() ? "true" : "false";
        else if (key == "elements" && j.peek() == '[') {
            j.next();
            while (j.peek() != ']') {
                if (j.next() != '{') break;
                MenuSystem::ParsedElement child;
                parse_element(j, child);
                el.elements.push_back(std::move(child));
                j.next(); // }
            }
            j.next(); // ]
        }
        else j.skip_value();
    }
}

static void parse_screen_body(MiniJson& j, MenuSystem::ParsedScreen& screen) {
    while (j.peek() != 0) {
        std::string key = j.read_string();
        if (key.empty()) break;
        if (j.next() != ':') break;
        if (key == "name") screen.name = j.read_string();
        else if (key == "overlay") screen.overlay = j.read_bool();
        else if (key == "background") screen.background = j.read_float_array();
        else if (key == "elements" && j.peek() == '[') {
            j.next();
            while (j.peek() != ']') {
                if (j.next() != '{') break;
                MenuSystem::ParsedElement el;
                parse_element(j, el);
                screen.elements.push_back(std::move(el));
                j.next();
            }
            j.next();
        }
        else j.skip_value();
    }
}

// --- Theme loading ---
static void parse_theme(MenuSystem::ParsedTheme& theme, MiniJson& j) {
    while (j.peek() != 0) {
        std::string key = j.read_string();
        if (key.empty()) break;
        if (j.next() != ':') break;
        if (key == "colors" && j.peek() == '{') {
            j.next();
            while (j.peek() != '}') {
                std::string cname = j.read_string();
                if (j.next() != ':') break;
                auto vals = j.read_float_array();
                if (vals.size() >= 4) theme.colors[cname] = {vals[0], vals[1], vals[2], vals[3]};
            }
            j.next();
        }
        else if (key == "fonts" && j.peek() == '{') {
            j.next();
            while (j.peek() != '}') {
                std::string fname = j.read_string();
                if (j.next() != ':') break;
                theme.fonts[fname] = j.read_number();
            }
            j.next();
        }
        else if (key == "spacing" && j.peek() == '{') {
            j.next();
            while (j.peek() != '}') {
                std::string sname = j.read_string();
                if (j.next() != ':') break;
                theme.spacing[sname] = j.read_number();
            }
            j.next();
        }
        else j.skip_value();
    }
}

}  // namespace

// ── MenuSystem ─────────────────────────────────────────────────────────────────

MenuSystem::MenuSystem() {}

bool MenuSystem::load_from_directory(const std::string& dir_path) {
    screen_dir_ = dir_path;
    screens_.clear();
    str_vars_.clear();
    float_vars_.clear();

    // Load theme first
    std::string theme_path = dir_path + "/theme.json";
    {
        std::ifstream f(theme_path);
        if (f.is_open()) {
            std::string content((std::istreambuf_iterator<char>(f)), {});
            MiniJson j(content);
            j.next(); // {
            parse_theme(theme_, j);
            log_info_cat(AE_LOG_CATEGORY, "Loaded theme (" + std::to_string(theme_.colors.size()) + " colors, " +
                          std::to_string(theme_.fonts.size()) + " fonts)");
        }
    }

    // Load all .json files except theme
    for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
        if (!entry.is_regular_file()) continue;
        std::string name = entry.path().filename().stem().string();
        if (name == "theme") continue;
        std::string ext = entry.path().extension().string();
        if (ext != ".json") continue;

        load_screen(entry.path().string(), name);
    }

    log_info_cat(AE_LOG_CATEGORY, "Loaded " + std::to_string(screens_.size()) + " menu screens from " + dir_path);
    return !screens_.empty();
}

bool MenuSystem::load_screen(const std::string& path, const std::string& name) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(f)), {});
    MiniJson j(content);
    j.next(); // {

    ParsedScreen screen;
    screen.name = name;
    screen.path = path;
    screen.last_write = std::filesystem::last_write_time(path);
    parse_screen_body(j, screen);

    screens_[name] = std::move(screen);
    return true;
}

void MenuSystem::register_action(const std::string& name, MenuAction fn) {
    actions_[name] = std::move(fn);
}

// ── Rendering ─────────────────────────────────────────────────────────────────

namespace {

struct LayoutPos {
    float x, y;
};

LayoutPos compute_position(float el_x, float el_y, const std::string& anchor,
                            float parent_x, float parent_y, float parent_w, float parent_h,
                            float el_w, float el_h) {
    LayoutPos p;
    float base_x = parent_x + el_x;
    float base_y = parent_y + el_y;

    if (anchor == "center")           { p.x = base_x - el_w * 0.5f; p.y = base_y - el_h * 0.5f; }
    else if (anchor == "top_center")  { p.x = base_x - el_w * 0.5f; p.y = base_y; }
    else if (anchor == "bottom_center") { p.x = base_x - el_w * 0.5f; p.y = base_y - el_h; }
    else if (anchor == "top_left")    { p.x = base_x; p.y = base_y; }
    else if (anchor == "top_right")   { p.x = base_x - el_w; p.y = base_y; }
    else { p.x = base_x; p.y = base_y; }
    return p;
}

}  // namespace

void MenuSystem::render() {
    if (screen_stack_.empty()) return;
    const std::string& name = screen_stack_.back();
    auto it = screens_.find(name);
    if (it == screens_.end()) return;

    const auto& screen = it->second;
    auto& io = ImGui::GetIO();
    float sw = io.DisplaySize.x;
    float sh = io.DisplaySize.y;

    // Background
    if (!screen.background.empty() && screen.background.size() >= 4) {
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({sw, sh});
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(screen.background[0], screen.background[1], screen.background[2], screen.background[3]));
        ImGui::Begin(("##bg_" + name).c_str(), nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::End();
        ImGui::PopStyleColor();
    }

    for (const auto& el : screen.elements) {
        render_element(el, 0, 0, sw, sh);
    }
}

void MenuSystem::render_element(const ParsedElement& el, float offset_x, float offset_y, float parent_w, float parent_h) {
    auto& io = ImGui::GetIO();
    float scale = io.DisplayFramebufferScale.x > 0 ? io.DisplayFramebufferScale.x : 1.0f;

    float el_w = el.width > 0 ? el.width * scale : 0;
    float el_h = el.height > 0 ? el.height * scale : 0;
    float btn_w = get_theme_float(theme_.spacing, "button_width", 280) * scale;
    float btn_h = get_theme_float(theme_.spacing, "button_height", 48) * scale;

    if (el.type == "spacer") {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + el.height * scale);
        return;
    }

    if (el.type == "separator") {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        return;
    }

    if (el.type == "panel") {
        auto pos = compute_position(el.x * parent_w, el.y * parent_h, el.anchor, offset_x, offset_y, parent_w, parent_h, el_w, el_h);

        if (el.color == "none") {
            ImGui::SetNextWindowPos({pos.x, pos.y});
            ImGui::SetNextWindowSize({el_w, el_h});
        } else {
            auto bg = get_theme_color(theme_.colors, el.color != "none" ? el.color : "panel");
            ImGui::PushStyleColor(ImGuiCol_WindowBg, bg);
            if (el.rounding > 0) ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, (float)el.rounding);
            ImGui::SetNextWindowPos({pos.x, pos.y});
            ImGui::SetNextWindowSize({el_w, el_h});
        }

        std::string win_name = "##panel_" + std::to_string(reinterpret_cast<uintptr_t>(&el));
        if (ImGui::Begin(win_name.c_str(), nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoBringToFrontOnFocus)) {
            for (const auto& child : el.elements) {
                render_element(child, pos.x, pos.y, el_w, el_h);
            }
        }
        ImGui::End();
        if (el.color != "none") {
            if (el.rounding > 0) ImGui::PopStyleVar();
            if (el.color != "none") ImGui::PopStyleColor();
        }
        return;
    }

    if (el.type == "text") {
        std::string text = resolve_variable(el.content);
        float font_size = get_theme_float(theme_.fonts, el.font, 1.0f);
        auto color = get_theme_color(theme_.colors, el.color, {1,1,1,1});

        float tw = ImGui::CalcTextSize(text.c_str()).x * font_size;
        auto pos = compute_position(el.x * parent_w, el.y * parent_h, el.anchor,
                                     offset_x, offset_y, parent_w, parent_h, tw, font_size * 20);

        ImGui::SetCursorScreenPos({pos.x, pos.y});
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        if (font_size != 1.0f) ImGui::SetWindowFontScale(font_size);
        ImGui::TextUnformatted(text.c_str());
        if (font_size != 1.0f) ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        return;
    }

    if (el.type == "button") {
        auto pos = compute_position(el.x * parent_w, el.y * parent_h, el.anchor,
                                     offset_x, offset_y, parent_w, parent_h, btn_w, btn_h);
        ImGui::SetCursorScreenPos({pos.x, pos.y});

        auto c_normal  = get_theme_color(theme_.colors, "button_" + el.style);
        auto c_hover   = get_theme_color(theme_.colors, "button_" + el.style + "_hover", c_normal);
        auto c_primary = get_theme_color(theme_.colors, "button_primary");

        if (el.style == "danger") {
            c_normal = get_theme_color(theme_.colors, "button_danger");
            c_hover = ImVec4(c_normal.x * 1.1f, c_normal.y * 0.9f, c_normal.z * 0.9f, 1.0f);
        }
        if (c_normal.x == 0 && c_normal.y == 0 && c_normal.z == 0) c_normal = c_primary;

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, c_normal);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, c_hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, c_normal);

        float font_scale = get_theme_float(theme_.fonts, "button", 1.05f);
        if (font_scale != 1.0f) ImGui::SetWindowFontScale(font_scale);

        bool clicked = ImGui::Button(el.label.c_str(), {btn_w, btn_h});

        if (font_scale != 1.0f) ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        if (clicked && !el.action.empty()) {
            execute_action(el.action);
        }
        return;
    }

    if (el.type == "map_card") {
        auto pos = compute_position(el.x * parent_w + offset_x, el.y * parent_h + offset_y, el.anchor,
                                     offset_x, offset_y, parent_w, parent_h, el_w, el_h);
        ImGui::SetCursorScreenPos({pos.x, pos.y});

        auto bg = get_theme_color(theme_.colors, "button_secondary");
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);

        std::string child_name = "##mapcard_" + el.map_id;
        if (ImGui::BeginChild(child_name.c_str(), {el_w, el_h}, ImGuiChildFlags_Border)) {
            auto accent = get_theme_color(theme_.colors, "accent");
            auto text1  = get_theme_color(theme_.colors, "text_primary");
            auto text2  = get_theme_color(theme_.colors, "text_secondary");

            ImGui::PushStyleColor(ImGuiCol_Text, accent);
            ImGui::SetWindowFontScale(1.1f);
            ImGui::TextUnformatted(el.label.c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, text2);
            ImGui::SetWindowFontScale(0.85f);
            ImGui::TextWrapped("%s", el.description.c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, text1);
            ImGui::TextUnformatted(el.players.c_str());
            ImGui::PopStyleColor();

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
            if (ImGui::Button(("SELECT##" + el.map_id).c_str(), {el_w - 16, 26})) {
                if (!el.action.empty()) execute_action(el.action);
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        return;
    }

    if (el.type == "progress_bar") {
        float bar_w = el.width > 0 ? el.width * scale : 300 * scale;
        float bar_h = el.height > 0 ? el.height * scale : 8 * scale;
        auto pos = compute_position(el.x * parent_w, el.y * parent_h, el.anchor,
                                     offset_x, offset_y, parent_w, parent_h, bar_w, bar_h);
        ImGui::SetCursorScreenPos({pos.x, pos.y});

        auto fill = get_theme_color(theme_.colors, "progress_fill");
        auto bg   = get_theme_color(theme_.colors, "progress_bg");
        float r = el.rounding > 0 ? (float)el.rounding : 4.0f;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled({pos.x, pos.y}, {pos.x + bar_w, pos.y + bar_h},
                          ImGui::ColorConvertFloat4ToU32(bg), r);
        float prog = std::clamp(loading_progress_, 0.0f, 1.0f);
        dl->AddRectFilled({pos.x, pos.y}, {pos.x + bar_w * prog, pos.y + bar_h},
                          ImGui::ColorConvertFloat4ToU32(fill), r);
        return;
    }

    if (el.type == "spinner") {
        float r = el.radius > 0 ? el.radius * scale : 10.0f * scale;
        float t = el.thickness > 0 ? el.thickness * scale : 2.5f * scale;
        auto pos = compute_position(el.x * parent_w, el.y * parent_h, el.anchor,
                                     offset_x, offset_y, parent_w, parent_h, r * 2, r * 2);
        pos.x += r;
        pos.y += r;

        auto col = get_theme_color(theme_.colors, el.color.empty() ? "accent" : el.color);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        spinner_angle_ += ImGui::GetIO().DeltaTime * 4.0f;
        const int segments = 30;
        for (int i = 0; i < segments; ++i) {
            float a = spinner_angle_ + (float)i / (float)segments * 2.0f * (float)M_PI;
            float fade = (float)i / (float)segments;
            ImU32 c = ImGui::ColorConvertFloat4ToU32({col.x, col.y, col.z, col.w * (0.2f + fade * 0.8f)});
            float a2 = a + 0.3f;
            dl->AddLine({pos.x + std::cos(a) * r * 0.5f, pos.y + std::sin(a) * r * 0.5f},
                        {pos.x + std::cos(a2) * r * 0.5f, pos.y + std::sin(a2) * r * 0.5f},
                        c, t);
        }
        return;
    }

    if (el.type == "docs_viewer") {
        // Determine active section index from float_vars convention.
        std::string var_name = "docs_active_" + std::to_string(reinterpret_cast<uintptr_t>(&el));
        auto var_it = float_vars_.find(var_name);
        int active_idx = var_it != float_vars_.end() ? static_cast<int>(var_it->second) : 0;
        active_idx = std::clamp(active_idx, 0, static_cast<int>(el.elements.size()) - 1);

        float el_w_scaled = el.width > 0 ? el.width * io.DisplayFramebufferScale.x : 800;
        float el_h_scaled = el.height > 0 ? el.height * io.DisplayFramebufferScale.y : 500;
        auto pos = compute_position(el.x * parent_w, el.y * parent_h, el.anchor,
                                     offset_x, offset_y, parent_w, parent_h, el_w_scaled, el_h_scaled);

        // Split: sidebar (left 220px) + content area
        float sidebar_w = 220.0f * io.DisplayFramebufferScale.x;
        float content_x = pos.x + sidebar_w + 4.0f;
        float content_w = el_w_scaled - sidebar_w - 4.0f;

        // ── Sidebar panel ──
        auto sidebar_bg = get_theme_color(theme_.colors, "panel");
        ImGui::PushStyleColor(ImGuiCol_ChildBg, sidebar_bg);
        ImGui::SetCursorScreenPos({pos.x, pos.y});
        std::string sbar_id = "##docs_sbar_" + std::to_string(reinterpret_cast<uintptr_t>(&el));
        if (ImGui::BeginChild(sbar_id.c_str(), {sidebar_w, el_h_scaled}, ImGuiChildFlags_Border)) {
            auto accent = get_theme_color(theme_.colors, "accent");
            auto text_primary = get_theme_color(theme_.colors, "text_primary");
            auto text_secondary = get_theme_color(theme_.colors, "text_secondary");
            auto btn_sec = get_theme_color(theme_.colors, "button_secondary");
            auto btn_sec_hover = get_theme_color(theme_.colors, "button_secondary_hover");

            // Title
            ImGui::PushStyleColor(ImGuiCol_Text, accent);
            ImGui::SetWindowFontScale(1.3f);
            ImGui::TextUnformatted("DOCS");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::Spacing();

            // Section buttons
            for (int i = 0; i < static_cast<int>(el.elements.size()); ++i) {
                bool is_active = (i == active_idx);
                ImGui::PushStyleColor(ImGuiCol_Button, is_active ? accent : btn_sec);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, is_active ? accent : btn_sec_hover);
                ImGui::PushStyleColor(ImGuiCol_Text, text_primary);
                ImGui::SetWindowFontScale(0.95f);
                float btn_w = sidebar_w - 16.0f;
                if (ImGui::Button((el.elements[i].label + "##" + sbar_id + "_" + std::to_string(i)).c_str(),
                                  {btn_w, 36.0f})) {
                    float_vars_[var_name] = static_cast<float>(i);
                }
                ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor(3);
                ImGui::Spacing();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        // ── Content panel ──
        ImGui::SetCursorScreenPos({content_x, pos.y});
        auto content_bg = get_theme_color(theme_.colors, "panel");
        ImGui::PushStyleColor(ImGuiCol_ChildBg, content_bg);
        std::string cid = "##docs_content_" + std::to_string(reinterpret_cast<uintptr_t>(&el));
        if (ImGui::BeginChild(cid.c_str(), {content_w, el_h_scaled}, ImGuiChildFlags_Border)) {
            if (active_idx >= 0 && active_idx < static_cast<int>(el.elements.size())) {
                render_element(el.elements[active_idx], content_x, pos.y, content_w, el_h_scaled);
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    if (el.type == "docs_section") {
        // Render section content: title + children
        auto accent = get_theme_color(theme_.colors, "accent");
        auto text_primary = get_theme_color(theme_.colors, "text_primary");

        ImGui::PushStyleColor(ImGuiCol_Text, accent);
        ImGui::SetWindowFontScale(1.5f);
        ImGui::TextUnformatted(el.label.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();

        for (const auto& child : el.elements) {
            render_element(child, offset_x, offset_y, parent_w, parent_h);
        }
        return;
    }

    if (el.type == "code_block") {
        // Simple syntax highlighting: keywords in magenta, strings green, numbers yellow, rest white
        auto text_primary = get_theme_color(theme_.colors, "text_primary");
        ImVec4 keyword_col = {0.85f, 0.25f, 0.85f, 1.0f}; // magenta
        ImVec4 string_col = {0.35f, 0.85f, 0.35f, 1.0f};  // green
        ImVec4 number_col = {0.90f, 0.80f, 0.20f, 1.0f};  // yellow
        ImVec4 comment_col = {0.45f, 0.50f, 0.55f, 1.0f}; // gray

        // C++ keywords for highlighting
        static const char* keywords[] = {
            "auto", "bool", "break", "case", "catch", "char", "class", "const", "constexpr",
            "continue", "default", "do", "double", "else", "enum", "explicit", "extern",
            "false", "float", "for", "friend", "goto", "if", "inline", "int", "long",
            "mutable", "namespace", "new", "noexcept", "nullptr", "operator", "private",
            "protected", "public", "return", "short", "signed", "sizeof", "static",
            "struct", "switch", "template", "this", "throw", "true", "try", "typedef",
            "typeid", "typename", "union", "unsigned", "using", "virtual", "void",
            "volatile", "while", "override", "final", "default", "delete", "include",
            "define", "ifdef", "endif", "pragma"
        };

        auto code_bg = ImVec4(0.07f, 0.08f, 0.12f, 0.95f);
        float avail_w = ImGui::GetContentRegionAvail().x;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, code_bg);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        std::string cb_id = "##code_" + std::to_string(reinterpret_cast<uintptr_t>(&el));

        // Language label
        if (!el.language.empty()) {
            auto text_secondary = get_theme_color(theme_.colors, "text_secondary");
            ImGui::PushStyleColor(ImGuiCol_Text, text_secondary);
            ImGui::SetWindowFontScale(0.75f);
            ImGui::TextUnformatted(el.language.c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
        }

        if (ImGui::BeginChild(cb_id.c_str(), {avail_w, 0}, ImGuiChildFlags_Border)) {
            ImGui::PushStyleColor(ImGuiCol_Text, text_primary);
            ImGui::PushFont(nullptr); // default font
            ImGui::SetWindowFontScale(0.90f);

            // Tokenize and render with syntax highlighting
            std::string code_str = el.content;
            const char* p = code_str.c_str();
            while (*p) {
                // Skip whitespace
                if (*p == ' ' || *p == '\t') {
                    const char* ws_start = p;
                    while (*p == ' ' || *p == '\t') ++p;
                    ImGui::TextUnformatted(std::string(ws_start, p - ws_start).c_str());
                    ImGui::SameLine(0, 0);
                    continue;
                }
                // Newline
                if (*p == '\n') {
                    ++p;
                    ImGui::TextUnformatted("");
                    continue;
                }
                // Comment (line)
                if (*p == '/' && *(p+1) == '/') {
                    const char* comment_start = p;
                    while (*p && *p != '\n') ++p;
                    ImGui::PushStyleColor(ImGuiCol_Text, comment_col);
                    ImGui::TextUnformatted(std::string(comment_start, p - comment_start).c_str());
                    ImGui::PopStyleColor();
                    if (*p == '\n') { ImGui::TextUnformatted(""); ++p; }
                    continue;
                }
                // String literal
                if (*p == '"') {
                    const char* str_start = p;
                    ++p; // skip opening "
                    while (*p && (*p != '"' || *(p-1) == '\\')) ++p;
                    if (*p == '"') ++p;
                    ImGui::PushStyleColor(ImGuiCol_Text, string_col);
                    ImGui::TextUnformatted(std::string(str_start, p - str_start).c_str());
                    ImGui::PopStyleColor();
                    ImGui::SameLine(0, 0);
                    continue;
                }
                // Number
                if (std::isdigit(*p) || (*p == '-' && std::isdigit(*(p+1)))) {
                    const char* num_start = p;
                    if (*p == '-') ++p;
                    while (std::isalnum(*p) || *p == '.' || *p == 'x' || *p == 'X' ||
                           *p == 'u' || *p == 'U' || *p == 'f' || *p == 'F') ++p;
                    ImGui::PushStyleColor(ImGuiCol_Text, number_col);
                    ImGui::TextUnformatted(std::string(num_start, p - num_start).c_str());
                    ImGui::PopStyleColor();
                    ImGui::SameLine(0, 0);
                    continue;
                }
                // Identifier / keyword
                if (std::isalpha(*p) || *p == '_' || *p == '#') {
                    const char* ident_start = p;
                    while (std::isalnum(*p) || *p == '_') ++p;
                    std::string token(ident_start, p - ident_start);
                    bool is_keyword = false;
                    for (const char* kw : keywords) {
                        if (token == kw) { is_keyword = true; break; }
                    }
                    if (is_keyword) {
                        ImGui::PushStyleColor(ImGuiCol_Text, keyword_col);
                    }
                    ImGui::TextUnformatted(token.c_str());
                    if (is_keyword) {
                        ImGui::PopStyleColor();
                    }
                    ImGui::SameLine(0, 0);
                    continue;
                }
                // Punctuation / operators: single char
                {
                    char single[2] = {*p, 0};
                    ImGui::TextUnformatted(single);
                    ImGui::SameLine(0, 0);
                    ++p;
                }
            }

            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor(); // text_primary
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(); // code_bg
        return;
    }

    if (el.type == "slider") {
        float w = el.width > 0 ? el.width * scale : 300 * scale;
        auto pos = compute_position(el.x * parent_w, el.y * parent_h, el.anchor,
                                     offset_x, offset_y, parent_w, parent_h, w, 40);
        ImGui::SetCursorScreenPos({pos.x, pos.y});
        ImGui::PushStyleColor(ImGuiCol_Text, get_theme_color(theme_.colors, "text_primary"));
        ImGui::TextUnformatted(el.label.c_str());
        ImGui::PopStyleColor();

        auto it = float_vars_.find(el.setting);
        float val = it != float_vars_.end() ? it->second : el.min;
        ImGui::SetCursorScreenPos({pos.x, pos.y + 18});
        ImGui::SetNextItemWidth(w);
        if (ImGui::SliderFloat(("##slider_" + el.setting).c_str(), &val, el.min, el.max, "%.2f")) {
            float_vars_[el.setting] = val;
            execute_action("setting_changed:" + el.setting);
        }
        return;
    }

    if (el.type == "toggle") {
        float w = el.width > 0 ? el.width * scale : 300 * scale;
        auto pos = compute_position(el.x * parent_w, el.y * parent_h, el.anchor,
                                     offset_x, offset_y, parent_w, parent_h, w, 30);
        ImGui::SetCursorScreenPos({pos.x, pos.y});
        ImGui::PushStyleColor(ImGuiCol_Text, get_theme_color(theme_.colors, "text_primary"));
        ImGui::TextUnformatted(el.label.c_str());
        ImGui::PopStyleColor();

        auto it = float_vars_.find(el.setting);
        bool val = (it != float_vars_.end() && it->second > 0.5f);
        ImGui::SetCursorScreenPos({pos.x + w - 40, pos.y});
        if (ImGui::Checkbox(("##tgl_" + el.setting).c_str(), &val)) {
            float_vars_[el.setting] = val ? 1.0f : 0.0f;
            execute_action("setting_changed:" + el.setting);
        }
        return;
    }
}

// ── Actions ───────────────────────────────────────────────────────────────────

void MenuSystem::execute_action(std::string_view action) {
    // Check for parameterized actions: "action:param"
    auto colon = action.find(':');
    std::string_view name = action.substr(0, colon);
    std::string_view param = (colon != std::string_view::npos) ? action.substr(colon + 1) : "";

    if (name == "push_screen") {
        push_screen(std::string(param));
        return;
    }
    if (name == "pop_screen") {
        pop_screen();
        return;
    }
    if (name == "pop_to_root") {
        pop_to_root();
        return;
    }
    if (name == "setting_changed") {
        // Handled internally by slider/toggle rendering above
        return;
    }

    auto it = actions_.find(std::string(name));
    if (it != actions_.end()) {
        it->second(param);
    }
}

void MenuSystem::push_screen(const std::string& name) {
    if (screens_.find(name) == screens_.end()) return;
    screen_stack_.push_back(name);
    current_screen_ = name;
}

void MenuSystem::pop_screen() {
    if (!screen_stack_.empty()) screen_stack_.pop_back();
    current_screen_ = screen_stack_.empty() ? "" : screen_stack_.back();
}

void MenuSystem::pop_to_root() {
    if (!screen_stack_.empty()) {
        screen_stack_.erase(screen_stack_.begin() + 1, screen_stack_.end());
        if (!screen_stack_.empty()) current_screen_ = screen_stack_[0];
    }
}

void MenuSystem::show_screen(const std::string& name) {
    screen_stack_.clear();
    push_screen(name);
}

void MenuSystem::set_active_screen(const std::string& name, bool active) {
    if (active) {
        if (screen_stack_.empty() || screen_stack_.back() != name) {
            push_screen(name);
        }
    } else {
        if (!screen_stack_.empty() && screen_stack_.back() == name) {
            pop_screen();
        }
    }
}

void MenuSystem::set_variable(const std::string& key, const std::string& value) {
    str_vars_[key] = value;
    if (key == "loading_progress") loading_progress_ = hex_to_float(value);
}

void MenuSystem::set_variable(const std::string& key, float value) {
    float_vars_[key] = value;
}

std::string MenuSystem::resolve_variable(const std::string& content) const {
    std::string result = content;
    for (const auto& [k, v] : str_vars_) {
        std::string token = "{" + k + "}";
        std::size_t pos = 0;
        while ((pos = result.find(token, pos)) != std::string::npos) {
            result.replace(pos, token.length(), v);
            pos += v.length();
        }
    }
    for (const auto& [k, v] : float_vars_) {
        std::string token = "{" + k + "}";
        std::string val = std::to_string(static_cast<int>(v));
        std::size_t pos = 0;
        while ((pos = result.find(token, pos)) != std::string::npos) {
            result.replace(pos, token.length(), val);
            pos += val.length();
        }
    }
    return result;
}

// ── Hot reload ────────────────────────────────────────────────────────────────

void MenuSystem::poll_hot_reload() {
    for (auto& [name, screen] : screens_) {
        if (!std::filesystem::exists(screen.path)) continue;
        auto current = std::filesystem::last_write_time(screen.path);
        if (current > screen.last_write) {
            screen.last_write = current;
            load_screen(screen.path, name);
            log_info_cat(AE_LOG_CATEGORY, "Hot-reloaded menu screen: " + name);
        }
    }
}

}  // namespace ae::ui
