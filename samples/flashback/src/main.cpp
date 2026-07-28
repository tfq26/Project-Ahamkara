// Flashback — independent game built on the Ahamkara engine.
//
// Flashback boots the Ahamkara client runtime (window, renderer, input,
// audio) and injects its own game module via the ae::IGameModule contract.
// This makes Flashback's identity, lifecycle, and ownership distinct from
// the "ahamkara_game" library while still consuming shared engine services.
//
// Usage (run from the repo root so the level's relative asset paths resolve):
//   ./build/<preset>/samples/flashback/flashback [--level <path-to.aelevel>]

#include "ahamkara/client/client_config.h"
#include "ahamkara/client/client_entry.h"
#include "ahamkara/client/controller_bindings.h"
#include "ae/core/log.h"
#include "flashback/game_module.h"

#include <cstdlib>
#include <memory>
#include <string>

namespace {

constexpr const char* kDefaultShowcaseLevel =
    "assets/compiled/levels/prototype_box.aelevel";

}  // namespace

int main(int argc, char** argv) {
    ae::init_file_logging("logs");
    const char* level_path = kDefaultShowcaseLevel;
    bool autoplay = false;
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--level") {
            level_path = argv[i + 1];
            break;
        }
    }
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--autoplay") {
            autoplay = true;
        }
    }

    // Defaults are fine for a demo; the real client loads these from config
    // files, but Flashback just showcases the engine running a level.
    ahamkara::client::ClientConfig client_config {};
    client_config.app_name = "Flashback";
    ahamkara::client::ControllerBindings controller_bindings {};
    const auto autoplay_scenario = autoplay
        ? ahamkara::client::make_default_autoplay_scenario(level_path)
        : ahamkara::client::PlaytestScenario {};

    ae::log_info(std::string("Flashback: booting the engine on level '") +
                 level_path + "'. (Run from the repo root so assets resolve.)");

    // Inject Flashback's own game module so the runtime lifecycle reflects
    // Flashback identity instead of the monolithic ahamkara_game library.
    // Ownership is transferred to the Application inside run_local_client,
    // so we release our unique_ptr after passing the raw pointer.
    auto game_module = flashback::create_flashback_game_module();
    ae::IGameModule* fb_module = game_module.release();

    const int result = run_local_client(
        client_config,
        controller_bindings,
        level_path,
        autoplay ? &autoplay_scenario : nullptr,
        fb_module);
    ae::shutdown_file_logging();
    return result;
}
