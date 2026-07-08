#include "ahamkara/game/maps/javelin4.h"

namespace ahamkara::game::maps {

const MapDefinition& javelin4() {
    static constexpr MapDefinition definition {
        "javelin4",
        "Javelin-4",
        MapCategory::Pvp,
        kDebugMapColliders.data(),
        kDebugMapColliders.size()
    };
    return definition;
}

}  // namespace ahamkara::game::maps
