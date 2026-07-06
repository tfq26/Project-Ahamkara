#include "ahamkara/game/worlds/debug_javelin4_world.h"

#include "ahamkara/game/maps/javelin4.h"

#include <array>

namespace ahamkara::game::worlds {

namespace {

constexpr std::array<TargetDummyState, 3> kTargetDummies {{
    {
        1,
        {0.0F, 1.5F, 3.0F},
        180.0F,
        150.0F,
        50.0F,
        50.0F,
        true,
        {0.0F, 1.5F, 3.0F}
    },
    {
        2,
        {6.0F, 1.15F, 7.0F},
        90.0F,
        150.0F,
        50.0F,
        50.0F,
        true,
        {6.0F, 1.15F, 7.0F},
        {1.0F, 0.0F, 0.0F},
        0.0F,
        3.0F,
        1.5F
    },
    {
        3,
        {-7.0F, 1.15F, -6.0F},
        270.0F,
        150.0F,
        50.0F,
        50.0F,
        true,
        {-7.0F, 1.15F, -6.0F},
        {0.0F, 0.0F, 1.0F},
        0.0F,
        4.0F,
        2.0F
    }
}};

constexpr std::array<InteractionTargetDefinition, 1> kInteractionTargets {{
    {
        1,
        {-10.5F, 0.0F, 0.0F},
        1.5F,
        true,
        "training_terminal"
    }
}};

}  // namespace

const WorldDefinition& debug_javelin4() {
    static constexpr WorldDefinition definition {
        "debug_javelin4",
        "Debug Javelin-4",
        nullptr,
        {{-12.0F, 0.0F, 0.0F}, 0.0F},
        kTargetDummies.data(),
        kTargetDummies.size(),
        kInteractionTargets.data(),
        kInteractionTargets.size()
    };
    static const WorldDefinition hydrated {
        definition.id,
        definition.display_name,
        &maps::javelin4(),
        definition.player_spawn,
        definition.target_dummies,
        definition.target_dummy_count,
        definition.interaction_targets,
        definition.interaction_target_count
    };
    return hydrated;
}

}  // namespace ahamkara::game::worlds
