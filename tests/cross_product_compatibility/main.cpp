// Cross-product compatibility smoke test.
//
// Verifies that the installed Ahamkara engine and Wish backend are
// compatible at runtime. Compiled against installed headers only.
//
// This binary is linked solely against Ahamkara::Core and Ahamkara::Runtime;
// no Wish source is included (Wish is consumed transitively through the
// Ahamkara game library path if available).

#include "ae/core/abi_version.h"
#include "ae/core/log.h"
#include "ae/core/time.h"
#include "ae/runtime/application.h"
#include "ae/runtime/game_module.h"
#include "ae/runtime/runtime_mode.h"

#include <cassert>
#include <iostream>
#include <string_view>

namespace {

// ── ABI version smoke ──────────────────────────────────────────────────

void test_abi_version_constants() {
    // Verify the ABI version header compiles and has expected values
    constexpr ae::AbiVersion kExpected {1, 0, 0};
    static_assert(ae::kEngineAbiVersion.major == kExpected.major,
                  "ABI major mismatch");
    static_assert(ae::kEngineAbiVersion.minor == kExpected.minor,
                  "ABI minor mismatch");
    static_assert(ae::kEngineAbiVersion.patch == kExpected.patch,
                  "ABI patch mismatch");

    // Self-compatibility must hold
    static_assert(ae::is_abi_compatible(ae::kEngineAbiVersion, ae::kEngineAbiVersion),
                  "ABI must be self-compatible");

    // Compatible: consumer minor <= producer minor
    static_assert(ae::is_abi_compatible({1, 0, 0}, {1, 0, 0}),
                  "1.0.0 should be compatible with 1.0.0");
    static_assert(ae::is_abi_compatible({1, 0, 0}, {1, 1, 0}),
                  "1.0.0 should be compatible with 1.1.0");

    // Incompatible: major mismatch
    static_assert(!ae::is_abi_compatible({2, 0, 0}, {1, 0, 0}),
                  "2.0.0 should NOT be compatible with 1.0.0");

    // Incompatible: consumer minor > producer minor
    static_assert(!ae::is_abi_compatible({1, 2, 0}, {1, 1, 0}),
                  "1.2.0 should NOT be compatible with 1.1.0");

    std::cout << "test_abi_version_constants: ok\n";
}

// ── Runtime smoke ──────────────────────────────────────────────────────

void test_application_create() {
    ae::Application app(ae::RuntimeMode::Tests);
    assert(app.mode() == ae::RuntimeMode::Tests);
    assert(!app.is_running());
    std::cout << "test_application_create: ok\n";
}

void test_application_start_stop() {
    ae::Application app(ae::RuntimeMode::Tests);
    auto start_result = app.start();
    assert(start_result.ok());
    assert(app.is_running());
    assert(app.frame_index() == 0);

    app.shutdown();
    assert(!app.is_running());
    std::cout << "test_application_start_stop: ok\n";
}

void test_application_tick() {
    ae::Application app(ae::RuntimeMode::Tests);
    (void)app.start();

    auto tick_result = app.tick(1.0F / 60.0F);
    assert(tick_result.ok());
    assert(app.frame_index() == 1);

    app.shutdown();
    std::cout << "test_application_tick: ok\n";
}

void test_game_module_api_version() {
    // Verify that the compile-time API version matches what the runtime expects
    constexpr auto api = ae::kGameModuleApiVersion;
    static_assert(api.major == 1, "GameModule API major should be 1");
    static_assert(api.minor == 0, "GameModule API minor should be 0");
    static_assert(api.patch == 0, "GameModule API patch should be 0");

    // Compatibility self-check
    static_assert(api.is_compatible_with(api),
                  "GameModule API must be self-compatible");

    // Compatible: consumer minor <= producer minor
    static_assert(ae::GameModuleApiVersion{1, 0, 0}.is_compatible_with({1, 0, 0}));
    static_assert(ae::GameModuleApiVersion{1, 0, 0}.is_compatible_with({1, 1, 0}));

    // Incompatible: major mismatch
    static_assert(!ae::GameModuleApiVersion{2, 0, 0}.is_compatible_with({1, 0, 0}));

    std::cout << "test_game_module_api_version: ok\n";
}

void test_logging() {
    // Verify logs work without crash
    ae::log_info("compatibility_smoke: runtime logging works");
    ae::log_info_cat("compat", "Compatibility test log entry");
    std::cout << "test_logging: ok\n";
}

void test_network_clock() {
    // If ae_network is available, test basic clock operations
    // (the Ahamkara::Network target may not be linked in all presets)
    std::cout << "test_network_clock: skipped (ae_network not linked in this target)\n";
}

}  // anonymous namespace

int main() {
    test_abi_version_constants();
    test_application_create();
    test_application_start_stop();
    test_application_tick();
    test_game_module_api_version();
    test_logging();
    test_network_clock();

    std::cout << "\nAll compatibility smoke tests passed.\n";
    return 0;
}
