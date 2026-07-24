#include "wish/core/engine_identity.h"
#include "wish/core/version.h"

namespace wish::core {

const EngineIdentity& identity() noexcept {
    static const EngineIdentity kIdentity{"Wish Engine", kWishVersionString};
    return kIdentity;
}

}  // namespace wish::core
