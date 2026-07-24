#define WISH_LOG_CATEGORY "EngineIdentity"

#include "wish/core/engine_identity.h"
#include "wish/log.h"

namespace wish::core {

const EngineIdentity& identity() noexcept {
    static const EngineIdentity kIdentity{"Wish Engine", "0.1"};
    wish::log_info_cat(WISH_LOG_CATEGORY, "Engine identity: " + std::string(kIdentity.name) +
                       " v" + std::string(kIdentity.version));
    return kIdentity;
}

}  // namespace wish::core
