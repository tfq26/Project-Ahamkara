#include "wish/integrations/nakama/nakama_bridge.h"

namespace wish::integrations::nakama {

bool is_enabled(const BridgeSettings& settings) noexcept {
    return settings.enabled;
}

}  // namespace wish::integrations::nakama
