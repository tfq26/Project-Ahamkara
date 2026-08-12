#pragma once

#include "ae/ui/menu_navigation_model.h"
#include "imgui.h"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ae::ui {

using MenuAction = std::function<void(std::string_view param)>;

class MenuSystem {
public:
    MenuSystem();

    bool load_from_directory(const std::string& dir_path);
    void register_action(const std::string& name, MenuAction fn);
    void render();
    void poll_hot_reload();

    void push_screen(const std::string& name);
    void pop_screen();
    void pop_to_root();
    void show_screen(const std::string& name);
    /// Hide all screens (used when entering gameplay).
    void clear_screens();
    [[nodiscard]] const std::string& current_screen() const { return current_screen_; }
    [[nodiscard]] bool is_visible() const { return !screen_stack_.empty(); }

    /// Focus state of the active screen (index into the ordered focusable
    /// list; -1 when nothing is enabled). Exposed for tests and tooling.
    [[nodiscard]] int focused_index() const {
        return nav_model_.focus_index();
    }
    [[nodiscard]] bool has_focus() const {
        return nav_model_.has_focus();
    }

    void set_variable(const std::string& key, const std::string& value);
    void set_variable(const std::string& key, float value);
    [[nodiscard]] const std::unordered_map<std::string, float>& float_vars() const { return float_vars_; }
    void set_active_screen(const std::string& name, bool active);

    struct ParsedElement {
        std::string type;
        std::string content;
        std::string label;
        std::string action;
        std::string style;
        std::string color;
        std::string font;
        std::string setting;
        std::string map_id;
        std::string description;
        std::string players;
        std::string id;         // docs section id
        std::string language;   // code block language
        std::string anchor;
        // Static enablement. A button is focusable/selectable only when
        // enabled (dynamic enablement can be driven through `enabled_var`).
        bool enabled {true};
        // Float variable name that gates enablement (value >= 0.5 => enabled).
        std::string enabled_var;
        float x{0}, y{0};
        float width{0}, height{0};
        float min{0}, max{0};
        float radius{0}, thickness{0};
        int rounding{0};
        std::vector<ParsedElement> elements;
    };

    struct ParsedScreen {
        std::string name;
        std::string path;
        std::filesystem::file_time_type last_write;
        bool overlay{false};
        std::vector<float> background{0.0f, 0.0f, 0.0f, 0.0f};
        std::vector<ParsedElement> elements;
    };

    struct ParsedTheme {
        std::unordered_map<std::string, ImVec4> colors;
        std::unordered_map<std::string, float> fonts;
        std::unordered_map<std::string, float> spacing;
    };

    bool load_screen(const std::string& path, const std::string& name);
    void render_element(const ParsedElement& el, float offset_x, float offset_y, float parent_w, float parent_h);
    void execute_action(std::string_view action);
    std::string resolve_variable(const std::string& content) const;

private:
  /// Collect focusable elements (buttons / map cards) in render order.
  void collect_focusables(const std::vector<ParsedElement>& elements,
                          std::vector<const ParsedElement*>& out);
  /// Keep the navigation model in sync with the current screen's elements.
  void sync_navigation_model(const ParsedScreen& screen);
  /// Apply keyboard/gamepad navigation input for the current frame.
  void handle_navigation_input();
  /// Build a focus ring around a focused element's item rect.
  void draw_focus_ring(const ImVec2& item_min, const ImVec2& item_max, float thickness);

  ParsedTheme theme_;
  std::unordered_map<std::string, ParsedScreen> screens_;
  std::vector<std::string> screen_stack_;
  std::string current_screen_;
  std::string screen_dir_;

  std::unordered_map<std::string, MenuAction> actions_;
  std::unordered_map<std::string, std::string> str_vars_;
  std::unordered_map<std::string, float> float_vars_;

  // Deterministic focus/navigation state for the active screen.
  MenuNavigationModel nav_model_ {};
  std::vector<const ParsedElement*> focus_list_ {};
  std::unordered_map<const ParsedElement*, int> focus_map_ {};
  std::string nav_screen_name_ {};
  int pending_activate_ {-1};

  // UI scale derived from the display size so menus stay usable across the
  // supported desktop window sizes.
  float ui_scale_ {1.0F};

  float loading_progress_ {0.0f};
  float spinner_angle_ {0.0f};
};

}  // namespace ae::ui
