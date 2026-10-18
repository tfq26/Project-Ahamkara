#pragma once

#include "imgui.h"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ae::ui {

using MenuAction = std::function<void(std::string_view param)>;

// Menu assets use normalized coordinates in the inclusive [-1, 1] range and
// pixel offsets outside that range. This allows centered layouts such as
// x=0.5 to coexist with fixed spacing such as y=54.
[[nodiscard]] constexpr float menu_coordinate_to_pixels(float coordinate,
                                                        float parent_extent) {
    return coordinate >= -1.0f && coordinate <= 1.0f
               ? coordinate * parent_extent
               : coordinate;
}

class MenuSystem {
public:
    MenuSystem();
    ~MenuSystem();

    bool load_from_directory(const std::string& dir_path);
    void register_action(const std::string& name, MenuAction fn);
    void render();
    void poll_hot_reload();

    void push_screen(const std::string& name);
    void pop_screen();
    void pop_to_root();
    void show_screen(const std::string& name);
    [[nodiscard]] const std::string& current_screen() const { return current_screen_; }
    [[nodiscard]] bool is_visible() const { return !screen_stack_.empty(); }

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
        std::string id;       // docs section id
        std::string language; // code block language
        std::string anchor;
        float x{0}, y{0};
        float width{0}, height{0};
        float min{0}, max{0};
        float radius{0}, thickness{0};
        int rounding{0};
        std::vector<ParsedElement> elements;
    };

    struct ParsedScreen {
        struct BackgroundLayer {
            std::string image;
            float drift {0.0f};
            float zoom {1.0f};
            float phase {0.0f};
            float opacity {1.0f};
            // Normalized vertical placement after a layer is cropped to the
            // viewport. Positive values move transparent mid/foreground art
            // down while preserving the sky behind it.
            float offset_y {0.0f};
        };

        std::string name;
        std::string path;
        std::string background_image;
        std::filesystem::file_time_type last_write;
        bool overlay{false};
        // Optional lightweight presentation layer for authored menu artwork.
        // This deliberately avoids a video dependency: it animates the crop,
        // atmospheric motes, and vignette at UI frame cost only.
        bool cinematic_background {false};
        float background_drift {0.0f};
        float background_zoom {1.0f};
        // Ordered back-to-front, alpha-bearing imagery for a lightweight
        // multi-plane cinematic background. Screens without layers retain the
        // legacy background_image behavior.
        std::vector<BackgroundLayer> background_layers;
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
  struct MenuTexture {
      unsigned int id {0};
      int width {0};
      int height {0};
  };

  const MenuTexture* texture_for_path(const std::string& path);

  ParsedTheme theme_;
  std::unordered_map<std::string, ParsedScreen> screens_;
  std::vector<std::string> screen_stack_;
  std::string current_screen_;
  std::string screen_dir_;

  std::unordered_map<std::string, MenuAction> actions_;
  std::unordered_map<std::string, std::string> str_vars_;
  std::unordered_map<std::string, float> float_vars_;
  std::unordered_map<std::string, MenuTexture> textures_;

  float loading_progress_ {0.0f};
  float spinner_angle_ {0.0f};
};

}  // namespace ae::ui
