/// \file
/// Entry point for the ahamkara_editor executable.
///
/// Launches the minimal editor window with ImGui UI, engine version info,
/// and a quit button.  Reuses ae_platform (GLFW) and ae_ui (ImGui) modules.

#include "ahamkara/editor/editor_window.h"

#include "ae/core/log.h"

#include "imgui.h"

#include <cstdlib>
#include <iostream>

// ── Helpers ──────────────────────────────────────────────────────────────────

namespace {

void print_usage(const char* prog) {
    std::cerr
        << "Usage: " << prog << " [options]\n"
        << "Options:\n"
        << "  --help, -h       Print this help message\n"
        << "  --version, -v    Print editor version\n";
}

void print_version() {
    std::cout
        << "Ahamkara Editor 0.1.0-dev\n"
        << "Engine: Ahamkara 0.1.0\n"
        << "ImGui:  " << IMGUI_VERSION << "\n";
}

}  // namespace

// ── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    // Parse trivial command-line flags.
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            print_usage(argc > 0 ? argv[0] : "ahamkara_editor");
            return EXIT_SUCCESS;
        }
        if (arg == "--version" || arg == "-v") {
            print_version();
            return EXIT_SUCCESS;
        }
    }

    // Initialise logging from environment (AE_LOG, AE_LOG_LEVEL).
    ae::init_log_levels_from_env();

    ahamkara::editor::EditorWindow editor;
    if (!editor.initialize()) {
        ae::log_error("Failed to initialise editor window.  Exiting.");
        return EXIT_FAILURE;
    }

    editor.run();
    return EXIT_SUCCESS;
}
