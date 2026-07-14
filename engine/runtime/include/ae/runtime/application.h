#pragma once

#include "ae/core/error_types.h"
#include "ae/runtime/game_module.h"
#include "ae/runtime/runtime_mode.h"

#include <cstdint>
#include <memory>

namespace ae {

class Application {
  public:
    explicit Application(RuntimeMode mode);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void set_game_module(std::unique_ptr<IGameModule> module);

    Result<void> start();
    Result<void> tick(float dt_seconds);
    void shutdown();

    [[nodiscard]] bool is_running() const {
        return running_;
    }
    [[nodiscard]] RuntimeMode mode() const {
        return runtime_mode_;
    }
    [[nodiscard]] IGameModule* game_module() const {
        return game_module_.get();
    }
    [[nodiscard]] std::uint64_t frame_index() const {
        return frame_index_;
    }

  private:
    RuntimeMode runtime_mode_;
    bool running_ {false};
    bool module_initialized_ {false};
    std::uint64_t frame_index_ {0};
    double time_seconds_ {0.0};
    std::unique_ptr<IGameModule> game_module_ {};
};

} // namespace ae
