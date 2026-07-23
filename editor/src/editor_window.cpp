#include "ahamkara/editor/editor_window.h"

#include "ae/core/log.h"
#include "ae/platform/window.h"
#include "ae/ui/ahamkara_ui.h"

#include "imgui.h"

#include <GLFW/glfw3.h>

#define AE_LOG_CATEGORY "Editor"

// ── Engine version (single source of truth) ──────────────────────────────────
//
// These are kept local to the editor for now.  When the engine grows a
// formal version API this block should be replaced by a call to that API.
namespace {

constexpr const char* kEngineVersion   = "0.1.0";
constexpr const char* kEngineName      = "Ahamkara";
constexpr const char* kEditorVersion   = "0.1.0-dev";

constexpr const char* kGlslVersion     = "#version 330";

}  // namespace

namespace ahamkara::editor {

// ── Construction / destruction ───────────────────────────────────────────────

EditorWindow::EditorWindow() = default;
EditorWindow::~EditorWindow() = default;

// ── Initialisation ───────────────────────────────────────────────────────────

bool EditorWindow::initialize(const std::string& title, int width, int height) {
    ae::WindowConfig config;
    config.title          = title;
    config.width          = width;
    config.height         = height;
    config.fullscreen     = false;
    config.create_opengl_context = true;

    window_ = ae::PlatformWindow::create(config);
    if (!window_) {
        ae::log_error_cat(AE_LOG_CATEGORY, "Failed to create editor window.");
        return false;
    }

    // Initialise the ae_ui (ImGui) layer on top of the GLFW window.
    // We pass the native GLFWwindow* that ae_ui expects.
    if (!ae::ui::initialize_ui(
            static_cast<GLFWwindow*>(window_->native_handle()),
            kGlslVersion)) {
        ae::log_error_cat(AE_LOG_CATEGORY, "Failed to initialise ImGui.");
        return false;
    }

    initialized_ = true;

    ae::log_info_cat(AE_LOG_CATEGORY,
                     "Editor window initialised (" + std::to_string(width)
                         + "x" + std::to_string(height) + ").");
    return true;
}

// ── Main loop ────────────────────────────────────────────────────────────────

void EditorWindow::run() {
    if (!initialized_) {
        ae::log_error_cat(AE_LOG_CATEGORY,
                          "EditorWindow::run() called without initialise().");
        return;
    }

    ae::log_info_cat(AE_LOG_CATEGORY, "Editor event loop started.");

    while (!quit_requested_) {
        // Process OS events; break if the window close button was clicked.
        if (!window_->poll_events()) {
            break;
        }

        // If the user clicked Quit in the window title bar or via OS shortcut,
        // honour it.
        if (window_->should_close()) {
            quit_requested_ = true;
            break;
        }

        render_frame();
    }

    ae::ui::shutdown_ui();
    ae::log_info_cat(AE_LOG_CATEGORY, "Editor shut down.");
}

// ── Frame rendering ──────────────────────────────────────────────────────────

void EditorWindow::render_frame() {
    // Let ae_ui sync GLFW input to ImGui and begin a new frame.
    ae::ui::sync_input_to_imgu(
        static_cast<GLFWwindow*>(window_->native_handle()));
    ae::ui::begin_ui_frame();

    // ── Engine Info panel ────────────────────────────────────────────────────
    //
    // This is the primary editor panel.  When ImGui docking support is
    // enabled project-wide (via IMGUI_ENABLE_DOCKING in imconfig.h), this
    // panel can be turned into a dockable space for tools such as the
    // Scene Viewport, Entity Inspector, and Asset Browser.
    ImGui::Begin("Engine Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::TextUnformatted(kEngineName);
    ImGui::Separator();
    ImGui::Text("Engine version : %s", kEngineVersion);
    ImGui::Text("Editor version : %s", kEditorVersion);
    ImGui::Text("ImGui version  : %s", IMGUI_VERSION);
    ImGui::Separator();
    ImGui::Text("Renderer : OpenGL 3.3");
    ImGui::Text("Platform : GLFW");

    // ── Quit button ──────────────────────────────────────────────────────────
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Button("Quit Editor", ImVec2(160.0F, 36.0F))) {
        quit_requested_ = true;
    }
    ImGui::End();

    // ── Demo window (toggle with Ctrl+Shift+D) ──────────────────────────────
    // Useful during development; can be removed in a production build.
    ImGui::ShowDemoWindow();

    // Finalise the ImGui frame and submit draw commands.
    ae::ui::end_ui_frame();

    // Swap buffers (the ae_ui layer renders via OpenGL).
    glfwSwapBuffers(
        static_cast<GLFWwindow*>(window_->native_handle()));
}

}  // namespace ahamkara::editor
