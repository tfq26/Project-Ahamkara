#include "flashback/game_module.h"

#include "ae/core/error_types.h"
#include "ae/runtime/game_module.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace flashback {

namespace {

class FlashbackGameModule final : public ae::IGameModule {
  public:
    std::string_view name() const override {
        return "Flashback";
    }

    ae::GameModuleApiVersion api_version() const override {
        return ae::kGameModuleApiVersion;
    }

    ae::Result<void> initialize(const ae::GameModuleHostServices& /*host*/) override {
        initialized_ = true;
        return {};
    }

    ae::Result<void> tick(const ae::GameModuleFrameContext& /*frame*/) override {
        return {};
    }

    void shutdown() override {
        shut_down_ = true;
    }

    [[nodiscard]] bool initialized() const { return initialized_; }
    [[nodiscard]] bool shut_down() const { return shut_down_; }

  private:
    bool initialized_ {false};
    bool shut_down_ {false};
};

}  // namespace

std::unique_ptr<ae::IGameModule> create_flashback_game_module() {
    return std::make_unique<FlashbackGameModule>();
}

}  // namespace flashback
