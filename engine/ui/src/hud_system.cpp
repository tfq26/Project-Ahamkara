#include "ae/ui/hud_system.h"

#include "ae/core/log.h"

#include <cctype>
#include <fstream>
#include <string>

#define AE_LOG_CATEGORY "UI"

namespace ae::ui {

namespace {

// Minimal JSON parser for HUD layout (reuses pattern from menu_system.cpp)
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
        try { return std::stof(s_.substr(start, p_ - start)); } catch (...) { return 0; }
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

}  // namespace

bool HudSystem::load(const std::string& path) {
    path_ = path;
    std::ifstream f(path);
    if (!f.is_open()) {
        ae::log_warning_cat(AE_LOG_CATEGORY, "Cannot open HUD layout: " + path);
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(f)), {});
    MiniJson j(content);
    j.next(); // {

    elements_.clear();

    while (j.peek() != 0) {
        std::string key = j.read_string();
        if (key.empty()) break;
        if (j.next() != ':') break;

        if (key == "elements" && j.peek() == '[') {
            j.next(); // [
            while (j.peek() != ']') {
                if (j.next() != '{') break;
                std::string type;
                float x = 0, y = 0, w = 0, h = 0;
                std::string anchor = "top_left";

                while (j.peek() != '}') {
                    std::string ek = j.read_string();
                    if (j.next() != ':') break;
                    if (ek == "type") type = j.read_string();
                    else if (ek == "x") x = j.read_number();
                    else if (ek == "y") y = j.read_number();
                    else if (ek == "width") w = j.read_number();
                    else if (ek == "height") h = j.read_number();
                    else if (ek == "anchor") anchor = j.read_string();
                    else j.skip_value();
                }
                j.next(); // }

                if (!type.empty()) {
                    auto el = HudElementRegistry::instance().create(type);
                    if (el) {
                        el->set_layout(x, y, anchor, w, h);
                        elements_.push_back({std::move(el), type});
                    }
                }
            }
            j.next(); // ]
        }
        else {
            j.skip_value();
        }
    }

    last_write_ = std::filesystem::last_write_time(path);
    ae::log_info_cat(AE_LOG_CATEGORY, "Loaded HUD layout: " + std::to_string(elements_.size()) + " elements");
    return true;
}

void HudSystem::render(float screen_w, float screen_h, const HudState& state) {
    for (auto& el : elements_) {
        if (el.element) el.element->render(screen_w, screen_h, state);
    }
}

void HudSystem::poll_hot_reload() {
    if (path_.empty()) return;
    if (!std::filesystem::exists(path_)) return;
    auto current = std::filesystem::last_write_time(path_);
    if (current > last_write_) {
        last_write_ = current;
        load(path_);
        ae::log_info_cat(AE_LOG_CATEGORY, "Hot-reloaded HUD layout: " + path_);
    }
}

}  // namespace ae::ui
