#pragma once

#include <memory>
#include <string>

namespace ae {
class PlatformWindow;
}  // namespace ae

namespace ahamkara::editor {

/// Minimal editor window that hosts the ImGui-based editor interface.
///
/// Owns the platform window (GLFW), initialises the ae_ui layer, and
/// runs the main event loop.  The editor layout displays engine version
/// information and a quit button; future iterations will add dockable
/// tool panels.
class EditorWindow {
public:
    EditorWindow();
    ~EditorWindow();

    EditorWindow(const EditorWindow&) = delete;
    EditorWindow& operator=(const EditorWindow&) = delete;
    EditorWindow(EditorWindow&&) = delete;
    EditorWindow& operator=(EditorWindow&&) = delete;

    /// Initialise the window and UI.  Returns true on success.
    bool initialize(const std::string& title = "Ahamkara Editor",
                    int width = 1280, int height = 720);

    /// Run the editor event loop.  Blocks until the window is closed.
    void run();

private:
    void render_frame();

    std::unique_ptr<ae::PlatformWindow> window_;
    bool initialized_ = false;
    bool quit_requested_ = false;
};

}  // namespace ahamkara::editor
