#include "ae/runtime/application.h"

#include "ae/core/error_registry.h"
#include "ae/core/log.h"
#include "ae/core/time.h"

namespace ae {
namespace {

Error make_runtime_error(std::string_view detail) {
    // Reuse CFG domain for host lifecycle misconfiguration until dedicated RUN codes expand.
    Error err(ae_cfg_0001());
    err.with_context("detail", detail);
    return err;
}

} // namespace

const char* to_string(RuntimeMode mode) {
    switch (mode) {
    case RuntimeMode::Client: return "Client";
    case RuntimeMode::DedicatedServer: return "DedicatedServer";
    case RuntimeMode::Editor: return "Editor";
    case RuntimeMode::AssetCooker: return "AssetCooker";
    case RuntimeMode::Tests: return "Tests";
    }
    return "Unknown";
}

Application::Application(RuntimeMode mode) : runtime_mode_(mode) {}
Application::~Application() {
    if (running_) {
        shutdown();
    }
}

void Application::set_game_module(std::unique_ptr<IGameModule> module) {
    if (running_) {
        ae::log_warning_cat("Runtime", "Ignoring set_game_module while running");
        return;
    }
    game_module_ = std::move(module);
}

Result<void> Application::start() {
    if (running_) {
        return make_runtime_error("application_already_running");
    }

    running_ = true;
    module_initialized_ = false;
    frame_index_ = 0;
    time_seconds_ = now_seconds();
    ae::log_info_cat("Runtime", std::string(to_string(runtime_mode_)) + " application started.");

    if (game_module_ != nullptr) {
        if (!game_module_->api_version().is_compatible_with(kGameModuleApiVersion)) {
            running_ = false;
            return make_runtime_error("game_module_api_incompatible");
        }
        GameModuleHostServices host;
        host.mode = runtime_mode_;
        auto init = game_module_->initialize(host);
        if (!init.ok()) {
            running_ = false;
            return init.error();
        }
        module_initialized_ = true;
        ae::log_info_cat("Runtime",
                         std::string("Game module initialized: ") + std::string(game_module_->name()));
    }
    return {};
}

Result<void> Application::tick(float dt_seconds) {
    if (!running_) {
        return make_runtime_error("application_not_running");
    }
    if (dt_seconds < 0.0F) {
        return make_runtime_error("negative_dt");
    }

    time_seconds_ += static_cast<double>(dt_seconds);
    ++frame_index_;

    if (game_module_ != nullptr && module_initialized_) {
        GameModuleFrameContext frame;
        frame.time_seconds = time_seconds_;
        frame.dt_seconds = dt_seconds;
        frame.frame_index = frame_index_;
        auto result = game_module_->tick(frame);
        if (!result.ok()) {
            return result.error();
        }
    }
    return {};
}

void Application::shutdown() {
    if (!running_) {
        return;
    }
    if (game_module_ != nullptr && module_initialized_) {
        game_module_->shutdown();
        module_initialized_ = false;
        ae::log_info_cat("Runtime",
                         std::string("Game module shutdown: ") + std::string(game_module_->name()));
    }
    running_ = false;
    ae::log_info_cat("Runtime", std::string(to_string(runtime_mode_)) + " application shutting down.");
}

} // namespace ae
