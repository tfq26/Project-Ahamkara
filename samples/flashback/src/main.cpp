// Flashback — engine demo.
//
// Flashback is intentionally a *thin shell* over the real engine/game/client
// stack. It boots the same local runtime as `ahamkara_client`
// (run_local_client: World simulation, ECS entities, level pipeline, mesh +
// sky/fog rendering, input/camera handling) pointed at a showcase level. Because
// it links `ahamkara_client_lib` rather than reimplementing gameplay, any engine
// improvement is reflected here automatically on the next build.
//
// Usage (run from the repo root so the level's relative asset paths resolve):
//   ./build/<preset>/samples/flashback/flashback [--level <path-to.aelevel>]

#include "ahamkara/client/client_config.h"
#include "ahamkara/client/client_entry.h"
#include "ahamkara/client/controller_bindings.h"
#include "ae/core/log.h"

#include <cstdlib>
#include <string>

namespace {

constexpr const char* kDefaultShowcaseLevel =
    "assets/compiled/levels/prototype_box.aelevel";

}  // namespace

int main(int argc, char** argv) {
    ae::init_file_logging("logs");
    const char* level_path = kDefaultShowcaseLevel;
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--level") {
            level_path = argv[i + 1];
            break;
        }
    }

    // Defaults are fine for a demo; the real client loads these from config
    // files, but Flashback just showcases the engine running a level.
    ahamkara::client::ClientConfig client_config {};
    ahamkara::client::ControllerBindings controller_bindings {};

    ae::log_info(std::string("Flashback demo: booting the engine on level '") +
                 level_path + "'. (Run from the repo root so assets resolve.)");

    const int result = run_local_client(client_config, controller_bindings, level_path);
    ae::shutdown_file_logging();
    return result;
}
