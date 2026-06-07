#include "wish/core/engine_identity.h"

namespace wish::core {

const EngineIdentity& identity() noexcept {
    static const EngineIdentity kIdentity{"Wish Engine", "0.1"};
    return kIdentity;
}

}  // namespace wish::core
