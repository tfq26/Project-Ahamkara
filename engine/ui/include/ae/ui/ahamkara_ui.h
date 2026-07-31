#pragma once

#include <cstdint>
#include <string>

struct GLFWwindow;

namespace ahamkara::client { struct ClientConfig; struct AudioConfig; }

namespace ae::ui {

enum class MenuScreen {
    None,
    MainMenu,
    Settings,
    Character,
    PauseOverlay,
    Docs
};

struct MenuState {
    bool visible {false};
    MenuScreen screen {MenuScreen::MainMenu};
    bool show_demo {false};

    // Settings editing copies (applied on confirm)
    int pending_width {1280};
    int pending_height {720};
    bool pending_fullscreen {false};
    float pending_gamma {1.0F};
    float pending_master_vol {1.0F};
    float pending_sfx_vol {1.0F};
    float pending_mouse_sens {1.0F};
    bool pending_audio_enabled {true};
};

bool initialize_ui(GLFWwindow* window, const char* glsl_version);
void shutdown_ui();
void begin_ui_frame();
void end_ui_frame();

// Feed current keyboard/mouse state from the application into ImGui IO.
// Call before begin_ui_frame() when using manual input forwarding (i.e. when
// ImGui was initialized with install_callbacks=false).
void sync_input_to_imgu(GLFWwindow* window);

bool wants_capture_mouse();
bool wants_capture_keyboard();

// Draw a gameplay crosshair in ImGui's foreground layer so it stays above the
// scene and any active menu windows.
void draw_crosshair_overlay();

// Menu rendering — returns true if game simulation should be paused
bool render_main_menu(MenuState& state, bool* quit_requested);
bool render_settings(MenuState& state);
bool render_character_sheet(const float* hp, float max_hp, int ammo, int max_ammo,
                            const char* weapon_name, int reserve_ammo);
bool render_pause_overlay(MenuState& state, bool* quit_to_menu);
bool render_docs_viewer(MenuState& state);

}  // namespace ae::ui
