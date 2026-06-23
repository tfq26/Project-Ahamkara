#pragma once

#include "ahamkara/client/client_config.h"
#include "ahamkara/client/controller_bindings.h"

#include <string>

// Client entry points (defined in debug_client.cpp / headless_clients.cpp).
//
// Declared here so the client executable AND engine demos (e.g.
// samples/flashback) can boot the real client runtime through
// `ahamkara_client_lib` instead of reimplementing it. This keeps demos in lock
// step with engine/game/client improvements automatically.

int run_local_client(
    ahamkara::client::ClientConfig& client_config,
    const ahamkara::client::ControllerBindings& controller_bindings,
    const char* level_path);

int run_windowed_client(const ahamkara::client::ClientConfig& client_config);
int run_sandbox_client(const char* level_path);
int run_network_client(const std::string& server_ip, int argc, char** argv);
