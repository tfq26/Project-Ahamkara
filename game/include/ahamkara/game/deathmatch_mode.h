#pragma once

#include "ahamkara/game/gameplay_types.h"

namespace ahamkara::game {

struct DeathmatchState {
    MatchState match;
    int kills_since_last_check = 0;

    void tick(float dt, const GameModeRules& rules);
    void on_kill(Team killer_team, ae::u32 killer_id);
    bool is_match_over() const;
    void reset();
};

} // namespace ahamkara::game
