#pragma once

#include "ae/core/error_types.h"
#include "ae/runtime/runtime_mode.h"

#include <cstdint>
#include <string_view>

namespace ae {

struct GameModuleApiVersion {
    std::uint16_t major {1};
    std::uint16_t minor {0};
    std::uint16_t patch {0};

    [[nodiscard]] constexpr bool is_compatible_with(GameModuleApiVersion host) const {
        return major == host.major &&
               (minor < host.minor || (minor == host.minor && patch <= host.patch));
    }
};

inline constexpr GameModuleApiVersion kGameModuleApiVersion {1, 0, 0};

struct GameModuleHostServices {
    RuntimeMode mode {RuntimeMode::Tests};
    void* user_data {nullptr};
};

struct GameModuleFrameContext {
    double time_seconds {0.0};
    float dt_seconds {0.0F};
    std::uint64_t frame_index {0};
};

class IGameModule {
public:
    virtual ~IGameModule() = default;

    [[nodiscard]] virtual std::string_view name() const = 0;
    [[nodiscard]] virtual GameModuleApiVersion api_version() const { return kGameModuleApiVersion; }
    virtual Result<void> initialize(const GameModuleHostServices& host) = 0;
    virtual Result<void> tick(const GameModuleFrameContext& frame) = 0;
    virtual void shutdown() = 0;
};

using GameModuleFactory = IGameModule* (*)();

} // namespace ae
