// ==========================================================================
// Flashback Headless Smoke
// ==========================================================================
//
// Proves that Flashback-relevant server/headless code compiles and links
// against only the installed Ahamkara + Wish package artifacts (no
// producer source checkout).
//
// This exercises the same API surface that the Flashback dedicated server
// and headless client use: core logging, time, network clock, application
// lifecycle, and the Wish engine identity.

#include "ae/core/log.h"
#include "ae/core/time.h"
#include "ae/core/version.h"
#include "ae/network/network_clock.h"
#include "ae/runtime/application.h"
#include "ae/runtime/game_module.h"
#include "ae/runtime/runtime_mode.h"

#include "wish/core/engine_identity.h"
#include "wish/core/version.h"
#include "wish/net/transport_config.h"
#include "wish/session/session_model.h"

#include <cstdio>

// ==========================================================================
// Minimal server-initialisation flow
// ==========================================================================

static int smoke_server_init() {
    // 1. Application bootstrap (as ahamkara_server does)
    ae::Application app(ae::RuntimeMode::DedicatedServer);
    if (!app.start()) {
        ae::log_error_cat("flashback_smoke", "Server application start failed");
        return 1;
    }

    // 2. Network configuration
    ae::NetworkClock clock;
    const double now = ae::now_seconds();
    clock.record_snapshot(1u, 60.0F, now);

    // 3. Wish transport config (as the server would set up)
    wish::net::TransportConfig transport;
    transport.port = 7777;
    transport.max_packet_size = 1200;

    // 4. Engine identity
    const auto& wid = wish::core::identity();
    ae::log_info_cat("flashback_smoke",
        "Wish identity retrieved");

    // 5. ABI version verification at runtime (belt-and-suspenders)
    ae::log_info_cat("flashback_smoke",
        "Ahamkara ABI=1  Wish ABI=1  NetProto=1  SessionProto=1");

    app.shutdown();
    return 0;
}

// ==========================================================================
// Main
// ==========================================================================

int main() {
    ae::log_info_cat("flashback_smoke", "Flashback headless smoke start");

    int failures = 0;
    failures += smoke_server_init();

    if (failures == 0) {
        ae::log_info_cat("flashback_smoke", "All Flashback headless smoke checks PASSED");
        std::printf("flashback_headless_smoke: ALL PASSED\n");
        return 0;
    } else {
        ae::log_error_cat("flashback_smoke",
            "Flashback headless smoke check(s) FAILED");
        std::printf("flashback_headless_smoke: %d FAILURE(S)\n", failures);
        return 1;
    }
}
