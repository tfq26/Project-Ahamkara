#pragma once

#include "ahamkara/client/client_config.h"
#include "ahamkara/client/controller_bindings.h"
#include "ahamkara/client/playtest_harness.h"

#include <string>

namespace ae { class IGameModule; }

// Client entry points (defined in debug_client.cpp / headless_clients.cpp).
//
// Declared here so the client executable AND engine demos (e.g.
// samples/flashback) can boot the real client runtime through
// `ahamkara_client_lib` instead of reimplementing it. This keeps demos in lock
// step with engine/game/client improvements automatically.
//
// The optional `game_module` parameter allows a product (e.g. Flashback) to
// inject its own ae::IGameModule implementation. When provided, the runtime
// will call its lifecycle methods (initialize/tick/shutdown) alongside the
// standard client pipeline.  Pass nullptr (the default) for the legacy
// monolithic client path.  The caller retains ownership for the lifetime of
// the call.

int run_local_client(
    ahamkara::client::ClientConfig& client_config,
    const ahamkara::client::ControllerBindings& controller_bindings,
    const char* level_path,
    const ahamkara::client::PlaytestScenario* autoplay_scenario = nullptr,
    ae::IGameModule* game_module = nullptr);

int run_windowed_client(const ahamkara::client::ClientConfig& client_config);
int run_sandbox_client(const char* level_path);
int run_playtest_client(const char* level_path, const ahamkara::client::PlaytestScenario& scenario);
int run_network_client(const std::string& server_ip, int argc, char** argv);
