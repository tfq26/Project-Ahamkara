#pragma once

#include "ae/runtime/game_module.h"

#include <memory>

namespace flashback {

/// Create a Flashback game module implementing the ae::IGameModule contract.
///
/// The returned module reports its identity as "Flashback" and satisfies the
/// standard GameModule lifecycle (initialize / tick / shutdown). This is the
/// primary boundary between the Ahamkara engine host and Flashback-specific
/// gameplay.
[[nodiscard]] std::unique_ptr<ae::IGameModule> create_flashback_game_module();

}  // namespace flashback
