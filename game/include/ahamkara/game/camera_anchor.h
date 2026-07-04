#pragma once

#include "ahamkara/game/net_types.h"

namespace ahamkara::game {

struct CameraAnchor {
    Vec3 position {};
    float yaw {0.0F};
    float pitch {0.0F};
};

}  // namespace ahamkara::game
