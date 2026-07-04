#include "ae/runtime/application.h"

#include "ae/core/log.h"

#include <string>
#include <string_view>

#define AE_LOG_CATEGORY "Runtime"

namespace ae {
namespace {

std::string_view runtime_mode_name(RuntimeMode mode) {
    switch (mode) {
        case RuntimeMode::Client:
            return "Client";
        case RuntimeMode::DedicatedServer:
            return "DedicatedServer";
        case RuntimeMode::Editor:
            return "Editor";
        case RuntimeMode::AssetCooker:
            return "AssetCooker";
        case RuntimeMode::Tests:
            return "Tests";
    }

    log_warning_cat(AE_LOG_CATEGORY, "runtime_mode_name: unknown mode " + std::to_string(static_cast<int>(mode)));
    return "Unknown";
}

}  // namespace

Application::Application(RuntimeMode runtime_mode)
    : runtime_mode_(runtime_mode) {
}

void Application::start() {
    running_ = true;
    log_info_cat(AE_LOG_CATEGORY, std::string(runtime_mode_name(runtime_mode_)) + " application started.");
}

void Application::shutdown() {
    if (!running_) {
        log_warning_cat(AE_LOG_CATEGORY, "shutdown called but application was not running");
        return;
    }

    running_ = false;
    log_info_cat(AE_LOG_CATEGORY, std::string(runtime_mode_name(runtime_mode_)) + " application shutting down.");
}

bool Application::is_running() const {
    return running_;
}

RuntimeMode Application::mode() const {
    return runtime_mode_;
}

}  // namespace ae
