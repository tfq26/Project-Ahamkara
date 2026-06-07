#pragma once

#include "ahamkara/game/net_types.h"

namespace ahamkara::game {

struct TransformComponent {
    Vec3 position {};
    Vec3 velocity {};
    float yaw {0.0F};
};

struct HealthComponent {
    float current {100.0F};
    float max {100.0F};
    float shield {0.0F};
    float max_shield {100.0F};
};

struct ProjectileComponent {
    ae::u32 owner_id {0};
    float damage {0.0F};
    bool is_hitscan {false};
};

struct LifetimeComponent {
    float remaining_seconds {0.0F};
};

struct MovementComponent {
    ae::u8 State {0};
};

struct LocalPlayerTag {};
struct TargetDummyTag {};
struct VisibleTag {};

} // namespace ahamkara::game
