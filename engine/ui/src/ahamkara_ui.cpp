#include "ae/ui/ahamkara_ui.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace ae::ui {
namespace {

bool g_initialized = false;

// --- Color palette -----------------------------------------------------------

static constexpr ImVec4 kAccent      {0.20F, 0.55F, 0.90F, 1.00F};  // blue
static constexpr ImVec4 kAccentHover {0.30F, 0.65F, 1.00F, 1.00F};
static constexpr ImVec4 kSuccess     {0.20F, 0.80F, 0.40F, 1.00F};
static constexpr ImVec4 kDanger      {0.85F, 0.25F, 0.25F, 1.00F};
static constexpr ImVec4 kWarning     {0.95F, 0.65F, 0.10F, 1.00F};
static constexpr ImVec4 kTextBright  {0.95F, 0.96F, 0.98F, 1.00F};
static constexpr ImVec4 kTextDim     {0.55F, 0.58F, 0.65F, 1.00F};
static constexpr ImVec4 kBgDark      {0.05F, 0.06F, 0.10F, 0.96F};
static constexpr ImVec4 kBgPanel     {0.07F, 0.09F, 0.14F, 0.94F};
static constexpr ImVec4 kBgCard      {0.09F, 0.11F, 0.18F, 1.00F};
static constexpr ImVec4 kBorder      {0.14F, 0.17F, 0.24F, 1.00F};

// --- Helpers -----------------------------------------------------------------

void TextCentered(const char* text) {
    float w = ImGui::GetContentRegionAvail().x;
    float tw = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (w - tw) * 0.5F);
    ImGui::TextUnformatted(text);
}

void TextCenteredColored(const char* text, ImVec4 color) {
    float w = ImGui::GetContentRegionAvail().x;
    float tw = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (w - tw) * 0.5F);
    ImGui::TextColored(color, "%s", text);
}

bool MenuButton(const char* label, const ImVec2& size, bool primary = false) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(24, 10));

    if (primary) {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.85F));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(kAccentHover.x, kAccentHover.y, kAccentHover.z, 0.95F));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(kAccent.x, kAccent.y, kAccent.z, 1.00F));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.18F, 0.20F, 0.28F, 0.70F));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.24F, 0.27F, 0.38F, 0.85F));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.20F, 0.23F, 0.33F, 0.95F));
    }

    bool clicked = ImGui::Button(label, size);

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
    return clicked;
}

void SectionHeader(const char* label) {
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();
}

void SliderSetting(const char* label, float* v, float v_min, float v_max,
                   const char* fmt = "%.1f", float label_width = 160.0F) {
    ImGui::TextUnformatted(label);
    ImGui::SameLine(label_width);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10.0F);
    ImGui::SliderFloat(("##" + std::string(label)).c_str(), v, v_min, v_max, fmt);
}

void CheckboxSetting(const char* label, bool* v, float label_width = 160.0F) {
    ImGui::TextUnformatted(label);
    ImGui::SameLine(label_width);
    ImGui::Checkbox(("##" + std::string(label)).c_str(), v);
}

void KeyBindingRow(const char* action, const char* binding) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(action);
    ImGui::TableSetColumnIndex(1);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::TextUnformatted(binding);
    ImGui::PopStyleColor();
}

// --- Animated background pulse -----------------------------------------------

float PulseAlpha() {
    double t = ImGui::GetTime();
    return 0.03F + 0.02F * (float)std::sin(t * 0.8);
}

}  // namespace

bool initialize_ui(GLFWwindow* window, const char* glsl_version) {
    if (g_initialized) return true;

    glfwMakeContextCurrent(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 8.0F;
    style.FrameRounding     = 4.0F;
    style.GrabRounding      = 4.0F;
    style.ScrollbarRounding = 6.0F;
    style.ChildRounding     = 8.0F;
    style.PopupRounding     = 6.0F;
    style.WindowBorderSize  = 0.0F;
    style.FrameBorderSize   = 1.0F;
    style.ItemSpacing       = ImVec2(8, 8);
    style.ItemInnerSpacing  = ImVec2(8, 6);
    style.IndentSpacing     = 22.0F;
    style.ScrollbarSize     = 10.0F;
    style.GrabMinSize       = 8.0F;

    style.Colors[ImGuiCol_WindowBg]             = kBgDark;
    style.Colors[ImGuiCol_ChildBg]              = kBgPanel;
    style.Colors[ImGuiCol_PopupBg]              = kBgPanel;
    style.Colors[ImGuiCol_Border]               = kBorder;
    style.Colors[ImGuiCol_BorderShadow]         = ImVec4(0,0,0,0);
    style.Colors[ImGuiCol_FrameBg]              = ImVec4(0.10F, 0.12F, 0.18F, 0.80F);
    style.Colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.14F, 0.17F, 0.24F, 0.90F);
    style.Colors[ImGuiCol_FrameBgActive]        = ImVec4(0.12F, 0.15F, 0.22F, 1.00F);
    style.Colors[ImGuiCol_TitleBg]              = ImVec4(0.06F, 0.08F, 0.12F, 1.00F);
    style.Colors[ImGuiCol_TitleBgActive]        = ImVec4(0.08F, 0.10F, 0.15F, 1.00F);
    style.Colors[ImGuiCol_Header]               = ImVec4(0.12F, 0.15F, 0.22F, 0.70F);
    style.Colors[ImGuiCol_HeaderHovered]        = ImVec4(0.16F, 0.20F, 0.30F, 0.85F);
    style.Colors[ImGuiCol_HeaderActive]         = ImVec4(0.14F, 0.18F, 0.27F, 1.00F);
    style.Colors[ImGuiCol_Button]               = ImVec4(0.16F, 0.18F, 0.26F, 0.70F);
    style.Colors[ImGuiCol_ButtonHovered]        = ImVec4(0.22F, 0.25F, 0.36F, 0.85F);
    style.Colors[ImGuiCol_ButtonActive]         = ImVec4(0.18F, 0.21F, 0.31F, 1.00F);
    style.Colors[ImGuiCol_Tab]                  = ImVec4(0.10F, 0.12F, 0.18F, 0.70F);
    style.Colors[ImGuiCol_TabHovered]           = ImVec4(0.18F, 0.22F, 0.32F, 0.85F);
    style.Colors[ImGuiCol_TabActive]            = ImVec4(0.15F, 0.19F, 0.28F, 1.00F);
    style.Colors[ImGuiCol_TabUnfocused]         = ImVec4(0.08F, 0.10F, 0.15F, 0.50F);
    style.Colors[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.12F, 0.15F, 0.22F, 0.80F);
    style.Colors[ImGuiCol_CheckMark]            = kAccent;
    style.Colors[ImGuiCol_SliderGrab]           = kAccent;
    style.Colors[ImGuiCol_SliderGrabActive]     = kAccentHover;
    style.Colors[ImGuiCol_Text]                 = kTextBright;
    style.Colors[ImGuiCol_TextDisabled]         = kTextDim;
    style.Colors[ImGuiCol_Separator]            = ImVec4(0.12F, 0.15F, 0.22F, 0.60F);
    style.Colors[ImGuiCol_TableBorderStrong]    = kBorder;
    style.Colors[ImGuiCol_TableBorderLight]     = ImVec4(0.10F, 0.12F, 0.19F, 0.50F);
    style.Colors[ImGuiCol_TableHeaderBg]        = ImVec4(0.08F, 0.10F, 0.16F, 0.90F);
    style.Colors[ImGuiCol_TableRowBg]           = ImVec4(0.06F, 0.08F, 0.12F, 0.50F);
    style.Colors[ImGuiCol_TableRowBgAlt]        = ImVec4(0.08F, 0.10F, 0.16F, 0.50F);

    ImGui_ImplGlfw_InitForOpenGL(window, false);  // manual input forwarding

    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
        fprintf(stderr, "[UI] ImGui_ImplOpenGL3_Init failed — menus will not render.\n");
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    fprintf(stderr, "[UI] ImGui initialized successfully with GLSL '%s'\n", glsl_version);
    g_initialized = true;
    return true;
}

void shutdown_ui() {
    if (!g_initialized) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    g_initialized = false;
}

void begin_ui_frame() {
    if (!g_initialized) return;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void end_ui_frame() {
    if (!g_initialized) return;
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool wants_capture_mouse() {
    if (!g_initialized) return false;
    return ImGui::GetIO().WantCaptureMouse;
}

bool wants_capture_keyboard() {
    if (!g_initialized) return false;
    return ImGui::GetIO().WantCaptureKeyboard;
}

// =============================================================================
// Input forwarding — maps GLFW input state into ImGui IO every frame.
// =============================================================================

static ImGuiKey MapGlfwToImGuiKey(int glfw_key) {
    // Alphanumeric
    if (glfw_key >= GLFW_KEY_A && glfw_key <= GLFW_KEY_Z)
        return (ImGuiKey)(ImGuiKey_A + (glfw_key - GLFW_KEY_A));
    if (glfw_key >= GLFW_KEY_0 && glfw_key <= GLFW_KEY_9)
        return (ImGuiKey)(ImGuiKey_0 + (glfw_key - GLFW_KEY_0));

    // Function keys
    if (glfw_key >= GLFW_KEY_F1 && glfw_key <= GLFW_KEY_F24)
        return (ImGuiKey)(ImGuiKey_F1 + (glfw_key - GLFW_KEY_F1));

    // Keypad
    if (glfw_key >= GLFW_KEY_KP_0 && glfw_key <= GLFW_KEY_KP_9)
        return (ImGuiKey)(ImGuiKey_Keypad0 + (glfw_key - GLFW_KEY_KP_0));

    switch (glfw_key) {
        case GLFW_KEY_TAB:          return ImGuiKey_Tab;
        case GLFW_KEY_LEFT:         return ImGuiKey_LeftArrow;
        case GLFW_KEY_RIGHT:        return ImGuiKey_RightArrow;
        case GLFW_KEY_UP:           return ImGuiKey_UpArrow;
        case GLFW_KEY_DOWN:         return ImGuiKey_DownArrow;
        case GLFW_KEY_PAGE_UP:      return ImGuiKey_PageUp;
        case GLFW_KEY_PAGE_DOWN:    return ImGuiKey_PageDown;
        case GLFW_KEY_HOME:         return ImGuiKey_Home;
        case GLFW_KEY_END:          return ImGuiKey_End;
        case GLFW_KEY_INSERT:       return ImGuiKey_Insert;
        case GLFW_KEY_DELETE:       return ImGuiKey_Delete;
        case GLFW_KEY_BACKSPACE:    return ImGuiKey_Backspace;
        case GLFW_KEY_SPACE:        return ImGuiKey_Space;
        case GLFW_KEY_ENTER:        return ImGuiKey_Enter;
        case GLFW_KEY_ESCAPE:       return ImGuiKey_Escape;
        case GLFW_KEY_APOSTROPHE:   return ImGuiKey_Apostrophe;
        case GLFW_KEY_COMMA:        return ImGuiKey_Comma;
        case GLFW_KEY_MINUS:        return ImGuiKey_Minus;
        case GLFW_KEY_PERIOD:       return ImGuiKey_Period;
        case GLFW_KEY_SLASH:        return ImGuiKey_Slash;
        case GLFW_KEY_SEMICOLON:    return ImGuiKey_Semicolon;
        case GLFW_KEY_EQUAL:        return ImGuiKey_Equal;
        case GLFW_KEY_LEFT_BRACKET:  return ImGuiKey_LeftBracket;
        case GLFW_KEY_BACKSLASH:    return ImGuiKey_Backslash;
        case GLFW_KEY_RIGHT_BRACKET: return ImGuiKey_RightBracket;
        case GLFW_KEY_GRAVE_ACCENT: return ImGuiKey_GraveAccent;
        case GLFW_KEY_CAPS_LOCK:    return ImGuiKey_CapsLock;
        case GLFW_KEY_SCROLL_LOCK:  return ImGuiKey_ScrollLock;
        case GLFW_KEY_NUM_LOCK:     return ImGuiKey_NumLock;
        case GLFW_KEY_PRINT_SCREEN: return ImGuiKey_PrintScreen;
        case GLFW_KEY_PAUSE:        return ImGuiKey_Pause;
        case GLFW_KEY_KP_DECIMAL:   return ImGuiKey_KeypadDecimal;
        case GLFW_KEY_KP_DIVIDE:    return ImGuiKey_KeypadDivide;
        case GLFW_KEY_KP_MULTIPLY:  return ImGuiKey_KeypadMultiply;
        case GLFW_KEY_KP_SUBTRACT:  return ImGuiKey_KeypadSubtract;
        case GLFW_KEY_KP_ADD:       return ImGuiKey_KeypadAdd;
        case GLFW_KEY_KP_ENTER:     return ImGuiKey_KeypadEnter;
        case GLFW_KEY_KP_EQUAL:     return ImGuiKey_KeypadEqual;
        case GLFW_KEY_LEFT_SHIFT:    return ImGuiKey_LeftShift;
        case GLFW_KEY_LEFT_CONTROL:  return ImGuiKey_LeftCtrl;
        case GLFW_KEY_LEFT_ALT:      return ImGuiKey_LeftAlt;
        case GLFW_KEY_LEFT_SUPER:    return ImGuiKey_LeftSuper;
        case GLFW_KEY_RIGHT_SHIFT:   return ImGuiKey_RightShift;
        case GLFW_KEY_RIGHT_CONTROL: return ImGuiKey_RightCtrl;
        case GLFW_KEY_RIGHT_ALT:     return ImGuiKey_RightAlt;
        case GLFW_KEY_RIGHT_SUPER:   return ImGuiKey_RightSuper;
        case GLFW_KEY_MENU:         return ImGuiKey_Menu;
        default:                    return ImGuiKey_None;
    }
}

void sync_input_to_imgu(GLFWwindow* window) {
    if (!g_initialized) return;

    ImGuiIO& io = ImGui::GetIO();

    // Display size
    int w, h;
    glfwGetWindowSize(window, &w, &h);
    int fb_w, fb_h;
    glfwGetFramebufferSize(window, &fb_w, &fb_h);
    io.DisplaySize = ImVec2((float)w, (float)h);
    if (w > 0 && h > 0)
        io.DisplayFramebufferScale = ImVec2((float)fb_w / (float)w, (float)fb_h / (float)h);

    // Delta time
    static double last_time = 0.0;
    double now = glfwGetTime();
    if (last_time == 0.0) last_time = now;
    io.DeltaTime = (float)(now - last_time);
    last_time = now;

    // Mouse position
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    io.AddMousePosEvent((float)mx, (float)my);

    // Mouse buttons
    for (int i = 0; i < 5; i++)
        io.AddMouseButtonEvent(i, glfwGetMouseButton(window, i) == GLFW_PRESS);

    // Keyboard — poll all supported keys
    static const int kAllKeys[] = {
        GLFW_KEY_TAB, GLFW_KEY_LEFT, GLFW_KEY_RIGHT, GLFW_KEY_UP, GLFW_KEY_DOWN,
        GLFW_KEY_PAGE_UP, GLFW_KEY_PAGE_DOWN, GLFW_KEY_HOME, GLFW_KEY_END,
        GLFW_KEY_INSERT, GLFW_KEY_DELETE, GLFW_KEY_BACKSPACE, GLFW_KEY_SPACE,
        GLFW_KEY_ENTER, GLFW_KEY_ESCAPE,
        GLFW_KEY_APOSTROPHE, GLFW_KEY_COMMA, GLFW_KEY_MINUS, GLFW_KEY_PERIOD,
        GLFW_KEY_SLASH, GLFW_KEY_SEMICOLON, GLFW_KEY_EQUAL,
        GLFW_KEY_LEFT_BRACKET, GLFW_KEY_BACKSLASH, GLFW_KEY_RIGHT_BRACKET,
        GLFW_KEY_GRAVE_ACCENT,
        GLFW_KEY_CAPS_LOCK, GLFW_KEY_SCROLL_LOCK, GLFW_KEY_NUM_LOCK,
        GLFW_KEY_PRINT_SCREEN, GLFW_KEY_PAUSE,
        GLFW_KEY_KP_0, GLFW_KEY_KP_1, GLFW_KEY_KP_2, GLFW_KEY_KP_3, GLFW_KEY_KP_4,
        GLFW_KEY_KP_5, GLFW_KEY_KP_6, GLFW_KEY_KP_7, GLFW_KEY_KP_8, GLFW_KEY_KP_9,
        GLFW_KEY_KP_DECIMAL, GLFW_KEY_KP_DIVIDE, GLFW_KEY_KP_MULTIPLY,
        GLFW_KEY_KP_SUBTRACT, GLFW_KEY_KP_ADD, GLFW_KEY_KP_ENTER, GLFW_KEY_KP_EQUAL,
        GLFW_KEY_LEFT_SHIFT, GLFW_KEY_LEFT_CONTROL, GLFW_KEY_LEFT_ALT, GLFW_KEY_LEFT_SUPER,
        GLFW_KEY_RIGHT_SHIFT, GLFW_KEY_RIGHT_CONTROL, GLFW_KEY_RIGHT_ALT, GLFW_KEY_RIGHT_SUPER,
        GLFW_KEY_MENU,
    };

    for (int key : kAllKeys) {
        ImGuiKey ik = MapGlfwToImGuiKey(key);
        if (ik != ImGuiKey_None)
            io.AddKeyEvent(ik, glfwGetKey(window, key) == GLFW_PRESS);
    }

    // Alphabet + numbers
    for (int k = GLFW_KEY_A; k <= GLFW_KEY_Z; k++)
        io.AddKeyEvent(MapGlfwToImGuiKey(k), glfwGetKey(window, k) == GLFW_PRESS);
    for (int k = GLFW_KEY_0; k <= GLFW_KEY_9; k++)
        io.AddKeyEvent(MapGlfwToImGuiKey(k), glfwGetKey(window, k) == GLFW_PRESS);
    for (int k = GLFW_KEY_F1; k <= GLFW_KEY_F12; k++)
        io.AddKeyEvent(MapGlfwToImGuiKey(k), glfwGetKey(window, k) == GLFW_PRESS);
}

// =============================================================================
// Main Menu
// =============================================================================

bool render_main_menu(MenuState& state, bool* quit_requested) {
    ImGuiIO& io = ImGui::GetIO();
    float win_w = 680.0F, win_h = 540.0F;

    ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - win_w) * 0.5F,
                                    (io.DisplaySize.y - win_h) * 0.5F));
    ImGui::SetNextWindowSize(ImVec2(win_w, win_h));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (!ImGui::Begin("MainMenu", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar)) {
        ImGui::PopStyleVar();
        ImGui::End();
        return false;
    }
    ImGui::PopStyleVar();

    // Full background with subtle gradient via colored child
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kBgDark);
    ImGui::BeginChild("MenuBg", ImVec2(win_w, win_h), ImGuiChildFlags_None);
    ImGui::PopStyleColor();

    // Top bar accent line
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImGui::GetWindowPos(),
        ImVec2(ImGui::GetWindowPos().x + win_w,
               ImGui::GetWindowPos().y + 3.0F),
        ImGui::ColorConvertFloat4ToU32(kAccent));

    ImGui::SetCursorPosY(50.0F);

    // Title section
    ImGui::PushStyleColor(ImGuiCol_Text, kTextBright);
    ImGui::SetWindowFontScale(2.8F);
    TextCentered("FLASHBACK");
    ImGui::SetWindowFontScale(1.0F);
    ImGui::PopStyleColor();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0F);

    ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
    ImGui::SetWindowFontScale(1.3F);
    TextCentered("ARENA");
    ImGui::SetWindowFontScale(1.0F);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::SetWindowFontScale(0.85F);
    TextCentered("Eliminate all hostiles — survive the arena");
    ImGui::SetWindowFontScale(1.0F);
    ImGui::PopStyleColor();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 24.0F);

    // Action buttons
    float btn_w = 280.0F, btn_h = 46.0F;
    float cx = (win_w - btn_w) * 0.5F;

    ImGui::SetCursorPosX(cx);
    if (MenuButton("PLAY", ImVec2(btn_w, btn_h), true)) {
        state.visible = false;
        state.screen = MenuScreen::None;
    }

    ImGui::Spacing();
    ImGui::SetCursorPosX(cx);
    if (MenuButton("SETTINGS", ImVec2(btn_w, 40.0F))) {
        state.screen = MenuScreen::Settings;
    }

    ImGui::Spacing();
    ImGui::SetCursorPosX(cx);
    if (MenuButton("QUIT TO DESKTOP", ImVec2(btn_w, 36.0F))) {
        if (quit_requested) *quit_requested = true;
    }

    // Footer
    ImGui::SetCursorPosY(win_h - 36.0F);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::SetWindowFontScale(0.7F);
    TextCentered("Ahamkara Engine  v1.0  —  Wish Protocol");
    ImGui::SetWindowFontScale(1.0F);
    ImGui::PopStyleColor();

    ImGui::EndChild();
    ImGui::End();
    return true;
}

// =============================================================================
// Settings
// =============================================================================

bool render_settings(MenuState& state) {
    ImGuiIO& io = ImGui::GetIO();
    float win_w = 760.0F, win_h = 520.0F;
    float sidebar_w = 180.0F;

    ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - win_w) * 0.5F,
                                    (io.DisplaySize.y - win_h) * 0.5F));
    ImGui::SetNextWindowSize(ImVec2(win_w, win_h));

    if (!ImGui::Begin("Settings", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar)) {
        ImGui::End();
        return false;
    }

    // Back button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15F, 0.18F, 0.28F, 0.60F));
    if (ImGui::Button("<  Back to Menu", ImVec2(140, 28))) {
        state.screen = MenuScreen::MainMenu;
        ImGui::PopStyleColor(2);
        ImGui::End();
        return false;
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine(win_w - 100.0F);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
    ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
    ImGui::SetWindowFontScale(1.2F);
    ImGui::TextUnformatted("SETTINGS");
    ImGui::SetWindowFontScale(1.0F);
    ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::Spacing();

    // Left sidebar — categories
    ImGui::BeginChild("Sidebar", ImVec2(sidebar_w, 0), ImGuiChildFlags_Border);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));

    static int selected_tab = 0;
    const char* tabs[] = {"Video", "Audio", "Controls", "Gameplay"};

    for (int i = 0; i < 4; ++i) {
        bool is_selected = (selected_tab == i);
        if (is_selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.25F));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.35F));
            ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12F, 0.15F, 0.22F, 0.40F));
            ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.15F, 0.5F));
        if (ImGui::Button(tabs[i], ImVec2(sidebar_w - 16, 36))) selected_tab = i;
        ImGui::PopStyleVar();

        ImGui::PopStyleColor(3);
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();

    ImGui::SameLine();

    // Right panel — scrollable settings content
    ImGui::BeginChild("SettingsContent", ImVec2(0, 0), ImGuiChildFlags_None);

    if (selected_tab == 0) { // Video
        SectionHeader("DISPLAY");
        const int resolutions[][2] = {{1280,720}, {1600,900}, {1920,1080}, {2560,1440}};
        static int res_idx = 0;
        ImGui::TextUnformatted("Resolution");
        ImGui::SameLine(160);
        ImGui::SetNextItemWidth(200);
        char res_label[32];
        snprintf(res_label, sizeof(res_label), "%dx%d", resolutions[res_idx][0], resolutions[res_idx][1]);
        if (ImGui::BeginCombo("##resolution", res_label)) {
            for (int i = 0; i < 4; ++i) {
                char lbl[32];
                snprintf(lbl, sizeof(lbl), "%dx%d", resolutions[i][0], resolutions[i][1]);
                if (ImGui::Selectable(lbl, res_idx == i)) {
                    res_idx = i;
                    state.pending_width  = resolutions[i][0];
                    state.pending_height = resolutions[i][1];
                }
            }
            ImGui::EndCombo();
        }
        CheckboxSetting("Fullscreen", &state.pending_fullscreen);
        SliderSetting("Brightness", &state.pending_gamma, 0.5F, 2.0F, "%.2f");
        SliderSetting("Mouse Sensitivity", &state.pending_mouse_sens, 0.1F, 5.0F, "%.1f");

        SectionHeader("ADVANCED");
        static bool vsync = true;
        CheckboxSetting("V-Sync", &vsync);
        static bool show_fps = false;
        CheckboxSetting("Show FPS Counter", &show_fps);
    }

    else if (selected_tab == 1) { // Audio
        SectionHeader("VOLUME");
        CheckboxSetting("Audio Enabled", &state.pending_audio_enabled);
        SliderSetting("Master Volume", &state.pending_master_vol, 0.0F, 1.0F, "%.0f%%");
        SliderSetting("SFX Volume", &state.pending_sfx_vol, 0.0F, 1.0F, "%.0f%%");

        SectionHeader("OUTPUT");
        static int audio_device = 0;
        const char* devices[] = {"System Default", "Speakers", "Headphones"};
        ImGui::TextUnformatted("Output Device");
        ImGui::SameLine(160);
        ImGui::SetNextItemWidth(220);
        if (ImGui::BeginCombo("##audio_dev", devices[audio_device])) {
            for (int i = 0; i < 3; ++i)
                if (ImGui::Selectable(devices[i], audio_device == i)) audio_device = i;
            ImGui::EndCombo();
        }
    }

    else if (selected_tab == 2) { // Controls
        SectionHeader("MOUSE & KEYBOARD");
        if (ImGui::BeginTable("Keybinds", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Binding", ImGuiTableColumnFlags_WidthFixed, 130.0F);
            ImGui::TableHeadersRow();

            KeyBindingRow("Move Forward",   "W");
            KeyBindingRow("Move Backward",  "S");
            KeyBindingRow("Strafe Left",    "A");
            KeyBindingRow("Strafe Right",   "D");
            KeyBindingRow("Jump",           "Space");
            KeyBindingRow("Sprint",         "L-Shift");
            KeyBindingRow("Crouch",         "L-Ctrl");
            KeyBindingRow("Slide",          "C");
            KeyBindingRow("Fire",           "LMB");
            KeyBindingRow("Reload",         "R");
            KeyBindingRow("Use / Interact", "E");
            KeyBindingRow("Scoreboard",     "Tab");
            KeyBindingRow("Pause / Menu",   "Esc");

            ImGui::EndTable();
        }
    }

    else if (selected_tab == 3) { // Gameplay
        SectionHeader("HUD");
        static bool hud_enabled = true;
        CheckboxSetting("Show HUD", &hud_enabled);
        static bool crosshair_enabled = true;
        CheckboxSetting("Show Crosshair", &crosshair_enabled);
        static bool minimap_enabled = true;
        CheckboxSetting("Show Minimap", &minimap_enabled);

        SectionHeader("COMBAT");
        static bool hitmarkers = true;
        CheckboxSetting("Hitmarkers", &hitmarkers);
        static bool damage_numbers = true;
        CheckboxSetting("Damage Numbers", &damage_numbers);
    }

    ImGui::EndChild();
    ImGui::End();
    return true;
}

// =============================================================================
// Character Sheet
// =============================================================================

bool render_character_sheet(const float* hp, float max_hp, int ammo, int max_ammo,
                            const char* weapon_name, int reserve_ammo) {
    ImGuiIO& io = ImGui::GetIO();
    float w = 500.0F, h = 420.0F;

    ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - w) * 0.5F,
                                    (io.DisplaySize.y - h) * 0.5F));
    ImGui::SetNextWindowSize(ImVec2(w, h));

    if (!ImGui::Begin("Character", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return false;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
    ImGui::SetWindowFontScale(1.3F);
    ImGui::TextUnformatted("LOADOUT");
    ImGui::SetWindowFontScale(1.0F);
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    // Health bar
    ImGui::TextUnformatted("Health");
    ImGui::SameLine(80);
    char hp_buf[64];
    snprintf(hp_buf, sizeof(hp_buf), "%.0f / %.0f", *hp, max_hp);
    ImGui::ProgressBar(*hp / max_hp, ImVec2(-1, 22), hp_buf);

    ImGui::Spacing();
    ImGui::Spacing();

    // Ammo
    ImGui::TextUnformatted("Ammo");
    ImGui::SameLine(80);
    char ammo_buf[64];
    snprintf(ammo_buf, sizeof(ammo_buf), "%d / %d  (Reserve: %d)", ammo, max_ammo, reserve_ammo);
    ImGui::TextUnformatted(ammo_buf);

    ImGui::Spacing();
    SectionHeader("EQUIPPED WEAPON");
    ImGui::PushStyleColor(ImGuiCol_Text, kTextBright);
    ImGui::TextUnformatted(weapon_name);
    ImGui::PopStyleColor();

    SectionHeader("STATS");
    if (ImGui::BeginTable("Stats", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Stat");
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 60.0F);
        auto stat = [](const char* name, int val) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(name);
            ImGui::TableSetColumnIndex(1); ImGui::Text("%d", val);
        };
        stat("Mobility",    75);
        stat("Resilience",  62);
        stat("Recovery",    58);
        stat("Discipline",  40);
        stat("Intellect",   35);
        stat("Strength",    50);
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (MenuButton("Back", ImVec2(120, 32))) {
        ImGui::End();
        return true;
    }

    ImGui::End();
    return false;
}

// =============================================================================
// Pause Overlay
// =============================================================================

bool render_pause_overlay(MenuState& state, bool* quit_to_menu) {
    ImGuiIO& io = ImGui::GetIO();

    // Full-screen dim overlay
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.55F));
    if (!ImGui::Begin("PauseBg", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus)) {
        ImGui::PopStyleColor();
        ImGui::End();
        return false;
    }
    ImGui::PopStyleColor();

    // Center the menu panel
    float panel_w = 340.0F, panel_h = 300.0F;
    ImGui::SetCursorPos(ImVec2((io.DisplaySize.x - panel_w) * 0.5F,
                                (io.DisplaySize.y - panel_h) * 0.5F));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, kBgPanel);
    ImGui::BeginChild("PausePanel", ImVec2(panel_w, panel_h),
        ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY);

    // Accent line
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImGui::GetCursorScreenPos(),
        ImVec2(ImGui::GetCursorScreenPos().x + panel_w,
               ImGui::GetCursorScreenPos().y + 3.0F),
        ImGui::ColorConvertFloat4ToU32(kAccent));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12.0F);

    ImGui::PushStyleColor(ImGuiCol_Text, kTextBright);
    ImGui::SetWindowFontScale(1.6F);
    TextCentered("PAUSED");
    ImGui::SetWindowFontScale(1.0F);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Spacing();

    float btn_w = 240.0F;
    float cx = (panel_w - btn_w) * 0.5F;

    ImGui::SetCursorPosX(cx);
    if (MenuButton("RESUME", ImVec2(btn_w, 42.0F), true)) {
        state.visible = false;
    }

    ImGui::Spacing();
    ImGui::SetCursorPosX(cx);
    if (MenuButton("SETTINGS", ImVec2(btn_w, 38.0F))) {
        state.screen = MenuScreen::Settings;
    }

    ImGui::Spacing();
    ImGui::SetCursorPosX(cx);
    if (MenuButton("CHARACTER", ImVec2(btn_w, 38.0F))) {
        state.screen = MenuScreen::Character;
    }

    ImGui::Spacing();
    ImGui::SetCursorPosX(cx);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(kDanger.x, kDanger.y, kDanger.z, 0.15F));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(kDanger.x, kDanger.y, kDanger.z, 0.35F));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(kDanger.x, kDanger.y, kDanger.z, 0.45F));
    ImGui::PushStyleColor(ImGuiCol_Text, kDanger);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0F);
    if (ImGui::Button("QUIT TO MENU", ImVec2(btn_w, 34.0F))) {
        if (quit_to_menu) *quit_to_menu = true;
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::End();
    return true;
}

}  // namespace ae::ui
