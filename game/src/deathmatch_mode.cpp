#include "ahamkara/game/deathmatch_mode.h"

namespace ahamkara::game {

void DeathmatchState::tick(float dt, const GameModeRules& rules) {
    match.tick(dt, rules);
}

void DeathmatchState::on_kill(Team killer_team, ae::u32 killer_id) {
    match.add_score(killer_team, killer_id, 1);
    kills_since_last_check++;
}

bool DeathmatchState::is_match_over() const {
    return match.phase == MatchPhase::MatchEnd || match.phase == MatchPhase::PostMatch;
}

void DeathmatchState::reset() {
    match = {};
    match.set_phase(MatchPhase::Warmup, 5.0F);
    kills_since_last_check = 0;
}

} // namespace ahamkara::game
