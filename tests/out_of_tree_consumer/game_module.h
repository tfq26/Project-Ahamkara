#pragma once

#include <ae/runtime/game_module.h>
#include <ae/core/error_types.h>

#include <cstdint>
#include <string_view>

class MinimalGameModule final : public ae::IGameModule {
public:
    MinimalGameModule() = default;

    std::string_view name() const override {
        return "MinimalGameModule";
    }

    ae::Result<void> initialize(const ae::GameModuleHostServices& host) override {
        (void)host;
        initialized_ = true;
        tick_count_ = 0;
        return ae::success();
    }

    ae::Result<void> tick(const ae::GameModuleFrameContext& frame) override {
        (void)frame;
        ++tick_count_;
        return ae::success();
    }

    void shutdown() override {
        initialized_ = false;
    }

    [[nodiscard]] bool initialized() const { return initialized_; }
    [[nodiscard]] std::uint64_t tick_count() const { return tick_count_; }

private:
    bool initialized_ = false;
    std::uint64_t tick_count_ = 0;
};
