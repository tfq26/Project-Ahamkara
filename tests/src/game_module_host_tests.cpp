#include "ae/core/error_registry.h"
#include "ae/core/error_types.h"
#include "ae/runtime/application.h"
#include "ae/runtime/game_module.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

static int g_failures = 0;
#define EXPECT_TRUE(cond)                                             \
    do {                                                              \
        if (!(cond)) {                                                \
            std::cerr << "FAIL " << __LINE__ << " " << #cond << "\n"; \
            ++g_failures;                                             \
        }                                                             \
    } while (0)

namespace {

class TestGameModule final : public ae::IGameModule {
  public:
    std::string_view name() const override {
        return "test_module";
    }

    ae::Result<void> initialize(const ae::GameModuleHostServices& host) override {
        initialized = true;
        host_mode = host.mode;
        events.push_back("init");
        return {};
    }

    ae::Result<void> tick(const ae::GameModuleFrameContext& frame) override {
        ticks.push_back(frame.frame_index);
        events.push_back("tick");
        last_dt = frame.dt_seconds;
        return {};
    }

    void shutdown() override {
        events.push_back("shutdown");
        shut_down = true;
    }

    bool initialized {false};
    bool shut_down {false};
    ae::RuntimeMode host_mode {ae::RuntimeMode::Tests};
    float last_dt {0.0F};
    std::vector<std::uint64_t> ticks;
    std::vector<std::string> events;
};

class FailingInitModule final : public ae::IGameModule {
  public:
    std::string_view name() const override {
        return "failing_init";
    }
    ae::Result<void> initialize(const ae::GameModuleHostServices&) override {
        return ae::Error(ae::ae_cfg_0001()).with_context("detail", "init_failed");
    }
    ae::Result<void> tick(const ae::GameModuleFrameContext&) override {
        return {};
    }
    void shutdown() override {}
};

} // namespace

int main() {
    // Happy-path lifecycle shared by headless host
    {
        auto module = std::make_unique<TestGameModule>();
        auto* raw = module.get();
        ae::Application app(ae::RuntimeMode::DedicatedServer);
        app.set_game_module(std::move(module));

        auto start = app.start();
        EXPECT_TRUE(start.ok());
        EXPECT_TRUE(app.is_running());
        EXPECT_TRUE(raw->initialized);
        EXPECT_TRUE(raw->host_mode == ae::RuntimeMode::DedicatedServer);

        auto t1 = app.tick(1.0F / 60.0F);
        auto t2 = app.tick(1.0F / 60.0F);
        EXPECT_TRUE(t1.ok());
        EXPECT_TRUE(t2.ok());
        EXPECT_TRUE(raw->ticks.size() == 2);
        EXPECT_TRUE(raw->ticks[0] == 1);
        EXPECT_TRUE(raw->ticks[1] == 2);

        app.shutdown();
        EXPECT_TRUE(!app.is_running());
        EXPECT_TRUE(raw->shut_down);
        EXPECT_TRUE(raw->events.size() >= 3);
        EXPECT_TRUE(raw->events.front() == "init");
        EXPECT_TRUE(raw->events.back() == "shutdown");
    }

    // Init failure should not leave host running
    {
        ae::Application app(ae::RuntimeMode::Tests);
        app.set_game_module(std::make_unique<FailingInitModule>());
        auto start = app.start();
        EXPECT_TRUE(!start.ok());
        EXPECT_TRUE(!app.is_running());
        EXPECT_TRUE(start.error().code().text() == "AE-CFG-0001");
    }

    // Host can run without a module (engine-owned tools/tests)
    {
        ae::Application app(ae::RuntimeMode::AssetCooker);
        auto start = app.start();
        EXPECT_TRUE(start.ok());
        EXPECT_TRUE(app.tick(0.016F).ok());
        app.shutdown();
        EXPECT_TRUE(!app.is_running());
    }

    if (g_failures != 0) {
        std::cerr << "game_module_host_tests failures=" << g_failures << "\n";
        return 1;
    }
    std::cout << "game_module_host_tests: ok\n";
    return 0;
}
