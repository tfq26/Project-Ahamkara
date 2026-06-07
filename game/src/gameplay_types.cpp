#include "ahamkara/game/gameplay_types.h"

namespace ahamkara::game {

// --- SpawnSelector ----------------------------------------------------------

bool SpawnSelector::select(
    const std::vector<SpawnPoint>& spawns,
    Team team,
    ae::u32 current_tick,
    Vec3& out_position,
    float& out_yaw) {
    
    int best_idx = -1;
    int max_priority = -999999;
    ae::u32 min_last_used = 0xFFFFFFFFU;

    for (int i = 0; i < static_cast<int>(spawns.size()); ++i) {
        const auto& sp = spawns[i];
        if (!sp.enabled || sp.team != team) continue;

        ae::u32 last_used = (i < kMaxLastUsed) ? m_last_used_tick[i] : 0;
        
        if (sp.priority > max_priority) {
            max_priority = sp.priority;
            min_last_used = last_used;
            best_idx = i;
        } else if (sp.priority == max_priority) {
            if (last_used < min_last_used) {
                min_last_used = last_used;
                best_idx = i;
            }
        }
    }

    if (best_idx != -1) {
        out_position = spawns[best_idx].position;
        out_yaw = spawns[best_idx].yaw;
        return true;
    }
    
    (void)current_tick;
    return false;
}

void SpawnSelector::mark_used(int index, ae::u32 tick) {
    if (index >= 0 && index < kMaxLastUsed) {
        m_last_used_tick[index] = tick;
    }
}

// --- MatchState -------------------------------------------------------------

bool MatchState::tick(float delta_seconds, const GameModeRules& rules) {
    bool changed = false;

    if (phase == MatchPhase::Lobby) {
        if (rules.auto_start) {
            set_phase(MatchPhase::Warmup, 15.0F);
            changed = true;
        }
        return changed;
    }

    if (phase == MatchPhase::InProgress || phase == MatchPhase::Overtime) {
        match_time += delta_seconds;

        // Check score limit
        bool score_limit_reached = false;
        if (rules.score_limit > 0) {
            if (static_cast<int>(team_score_red) >= rules.score_limit || 
                static_cast<int>(team_score_blue) >= rules.score_limit) {
                score_limit_reached = true;
            }
        }

        // Check time limit
        bool time_limit_reached = false;
        if (rules.time_limit_minutes > 0.0F) {
            if (match_time >= rules.time_limit_minutes * 60.0F) {
                time_limit_reached = true;
            }
        }

        if (phase == MatchPhase::InProgress) {
            if (score_limit_reached || time_limit_reached) {
                if (team_score_red == team_score_blue) {
                    set_phase(MatchPhase::Overtime, 0.0F);
                    changed = true;
                } else {
                    set_phase(MatchPhase::MatchEnd, 15.0F);
                    changed = true;
                }
            }
        } else if (phase == MatchPhase::Overtime) {
            if (team_score_red != team_score_blue) {
                set_phase(MatchPhase::MatchEnd, 15.0F);
                changed = true;
            }
        }
        return changed;
    }

    if (phase_timer > 0.0F) {
        phase_timer -= delta_seconds;
        if (phase_timer <= 0.0F) {
            phase_timer = 0.0F;
            
            if (phase == MatchPhase::Warmup) {
                set_phase(MatchPhase::Countdown, 3.0F);
            } else if (phase == MatchPhase::Countdown) {
                set_phase(MatchPhase::InProgress, 0.0F);
            } else if (phase == MatchPhase::RoundEnd) {
                if (current_round + 1 < rules.round_count) {
                    current_round++;
                    set_phase(MatchPhase::Countdown, 3.0F);
                } else {
                    set_phase(MatchPhase::MatchEnd, 15.0F);
                }
            } else if (phase == MatchPhase::MatchEnd) {
                set_phase(MatchPhase::PostMatch, 30.0F);
            } else if (phase == MatchPhase::PostMatch) {
                team_score_red = 0;
                team_score_blue = 0;
                std::fill(std::begin(individual_score), std::end(individual_score), 0);
                match_winner_id = 0;
                match_time = 0.0F;
                current_round = 0;
                set_phase(MatchPhase::Lobby, 0.0F);
            }
            changed = true;
        }
    }

    return changed;
}

void MatchState::set_phase(MatchPhase new_phase, float duration) {
    phase = new_phase;
    phase_timer = duration;
}

void MatchState::add_score(Team team, ae::u32 player_id, ae::u16 score) {
    if (team == Team::Red) {
        team_score_red += score;
    } else if (team == Team::Blue) {
        team_score_blue += score;
    }
    
    if (player_id < 12) {
        individual_score[player_id] += score;
    }
    
    int max_scorer = 0;
    int max_val = individual_score[0];
    for (int i = 1; i < 12; ++i) {
        if (individual_score[i] > max_val) {
            max_val = individual_score[i];
            max_scorer = i;
        }
    }
    if (max_val > 0) {
        match_winner_id = max_scorer;
    }
}

} // namespace ahamkara::game
