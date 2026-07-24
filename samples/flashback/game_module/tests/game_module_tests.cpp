// Flashback game module smoke test.
//
// Instantiates the Flashback game module and verifies contract compliance
// without any graphical boot.

#include "ae/core/error_types.h"
#include "ae/runtime/application.h"
#include "ae/runtime/game_module.h"
#include "flashback/game_module.h"

#include <iostream>
#include <memory>
#include <string_view>

static int g_failures = 0;
#define EXPECT_TRUE(cond)                                             \
    do {                                                              \
        if (!(cond)) {                                                \
            std::cerr << "FAIL " << __LINE__ << " " << #cond << "\n"; \
            ++g_failures;                                             \
        }                                                             \
    } while (0)

int main() {
    // ── Happy path: lifecycle contract compliance ──────────────────────
    {
        auto module = flashback::create_flashback_game_module();
        EXPECT_TRUE(module != nullptr);

        // Identity
        EXPECT_TRUE(module->name() == "Flashback");

        // API version matches host expectations
        EXPECT_TRUE(module->api_version().is_compatible_with(ae::kGameModuleApiVersion));

        // Initialize
        ae::GameModuleHostServices host {};
        host.mode = ae::RuntimeMode::Tests;
        auto init_result = module->initialize(host);
        EXPECT_TRUE(init_result.ok());

        // Tick
        ae::GameModuleFrameContext frame {};
        frame.time_seconds = 1.0;
        frame.dt_seconds = 1.0F / 60.0F;
        frame.frame_index = 1;
        auto tick_result = module->tick(frame);
        EXPECT_TRUE(tick_result.ok());

        // Shutdown
        module->shutdown();
    }

    // ── Multiple instances should be independent ───────────────────────
    {
        auto m1 = flashback::create_flashback_game_module();
        auto m2 = flashback::create_flashback_game_module();
        EXPECT_TRUE(m1 != nullptr);
        EXPECT_TRUE(m2 != nullptr);
        EXPECT_TRUE(m1->name() == m2->name());
    }

    // ── Application integration: game module injected into ae::Application ──
    // This mirrors what run_local_client does internally when a product like
    // Flashback provides its own game module.  Verifies that the Flashback
    // module survives the Application lifecycle (start/tick/shutdown) without
    // requiring any graphical boot.
    {
        auto module = flashback::create_flashback_game_module();
        EXPECT_TRUE(module != nullptr);

        ae::Application app(ae::RuntimeMode::Tests);
        app.set_game_module(std::move(module));
        EXPECT_TRUE(app.game_module() != nullptr);
        EXPECT_TRUE(app.game_module()->name() == "Flashback");

        auto start_result = app.start();
        EXPECT_TRUE(start_result.ok());

        // Tick a few frames to verify stability
        for (int i = 0; i < 5; ++i) {
            auto tick_result = app.tick(1.0F / 60.0F);
            EXPECT_TRUE(tick_result.ok());
            EXPECT_TRUE(app.is_running());
            EXPECT_TRUE(app.frame_index() == static_cast<std::uint64_t>(i + 1));
        }

        app.shutdown();
        EXPECT_TRUE(!app.is_running());
    }

    // ── Application integration: nullptr module is valid ────────────────
    {
        ae::Application app(ae::RuntimeMode::Tests);
        EXPECT_TRUE(app.game_module() == nullptr);
        auto start_result = app.start();
        EXPECT_TRUE(start_result.ok());
        for (int i = 0; i < 3; ++i) {
            EXPECT_TRUE(app.tick(1.0F / 60.0F).ok());
        }
        app.shutdown();
    }

    if (g_failures != 0) {
        std::cerr << "flashback_game_module_tests failures=" << g_failures << "\n";
        return 1;
    }
    std::cout << "flashback_game_module_tests: ok\n";
    return 0;
}
