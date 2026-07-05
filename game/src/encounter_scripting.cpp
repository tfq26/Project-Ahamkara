#include "ahamkara/game/encounter_scripting.h"

#include <algorithm>
#include <cmath>

namespace ahamkara::game {

// --- EncounterManager ---------------------------------------------------------

void EncounterManager::add_encounter(const EncounterDef& def) {
    // Check for duplicates
    for (const auto& d : defs_) {
        if (d.id == def.id) return;
    }
    defs_.push_back(def);

    // Build runtime state
    EncounterState state;
    state.id = def.id;
    state.phase = EncounterPhase::Inactive;

    for (const auto& obj : def.objectives) {
        ObjectiveState os;
        os.id = obj.id;
        state.objectives.push_back(os);
    }

    for (const auto& wave : def.waves) {
        SpawnWaveState ws;
        ws.id = wave.id;
        int total = 0;
        for (const auto& g : wave.groups) total += g.count;
        ws.total_to_spawn = total;
        state.waves.push_back(ws);
    }

    for (const auto& trig : def.triggers) {
        TriggerState ts;
        ts.id = trig.id;
        state.triggers.push_back(ts);
    }

    states_.push_back(state);
}

void EncounterManager::clear() {
    defs_.clear();
    states_.clear();
    active_waves_.clear();
}

bool EncounterManager::start_encounter(const std::string& id) {
    for (auto& state : states_) {
        if (state.id == id) {
            state.phase = EncounterPhase::Active;
            state.started = true;
            state.current_wave_index = 0;
            state.phase_timer = 0.0F;

            // Reset objectives
            for (auto& obj : state.objectives) {
                obj.complete = false;
                obj.failed = false;
                obj.kill_count = 0;
                obj.elapsed = 0.0F;
            }

            // Reset waves
            for (auto& ws : state.waves) {
                ws.active = false;
                ws.complete = false;
                ws.delay_timer = 0.0F;
                ws.spawn_timer = 0.0F;
                ws.current_group = 0;
                ws.enemies_spawned = 0;
                ws.enemies_alive = 0;
            }

            // Reset triggers
            for (auto& ts : state.triggers) {
                ts.fired = false;
                ts.timer = 0.0F;
            }

            // Start first wave immediately
            start_wave(state, 0);
            return true;
        }
    }
    return false;
}

void EncounterManager::start_all() {
    for (auto& state : states_) {
        if (!state.started) {
            start_encounter(state.id);
        }
    }
}

void EncounterManager::start_wave(EncounterState& state, int wave_index) {
    if (wave_index < 0 || wave_index >= static_cast<int>(state.waves.size())) return;

    auto& ws = state.waves[wave_index];
    ws.active = true;

    // Find the wave def to get delay
    for (const auto& def : defs_) {
        if (def.id != state.id) continue;
        if (wave_index < static_cast<int>(def.waves.size())) {
            ws.delay_timer = def.waves[wave_index].delay_before;
        }
        break;
    }

    state.current_wave_index = wave_index;
}

void EncounterManager::tick(float delta_seconds) {
    for (auto& state : states_) {
        if (state.phase != EncounterPhase::Active &&
            state.phase != EncounterPhase::BetweenWaves) continue;

        state.phase_timer += delta_seconds;

        // Tick active waves
        for (int i = 0; i < static_cast<int>(state.waves.size()); ++i) {
            auto& ws = state.waves[i];
            if (!ws.active || ws.complete) continue;

            // Find the wave def
            const SpawnWaveDef* wd = nullptr;
            for (const auto& def : defs_) {
                if (def.id != state.id) continue;
                if (i < static_cast<int>(def.waves.size())) {
                    wd = &def.waves[i];
                }
                break;
            }
            if (!wd) { ws.complete = true; continue; }

            tick_wave(state, ws, *wd, delta_seconds);
        }

        // Check triggers
        check_triggers(state, delta_seconds);

        // Check objectives
        check_objectives(state);

        // Check if all waves complete → encounter complete
        bool all_waves_done = true;
        for (const auto& ws : state.waves) {
            if (!ws.complete) { all_waves_done = false; break; }
        }
        if (all_waves_done && state.phase == EncounterPhase::Active) {
            state.phase = EncounterPhase::Complete;
        }
    }
}

void EncounterManager::tick_wave(EncounterState& state, SpawnWaveState& ws,
                                  const SpawnWaveDef& wd, float dt) {
    (void)state;

    // Handle delay before wave starts spawning
    if (ws.delay_timer > 0.0F) {
        ws.delay_timer -= dt;
        return;
    }

    if (wd.simultaneous) {
        // All enemies spawn immediately
        ws.complete = true;
        ws.enemies_spawned = ws.total_to_spawn;
        ws.enemies_alive = ws.total_to_spawn;
        return;
    }

    // Staggered spawning
    ws.spawn_timer -= dt;
    if (ws.spawn_timer <= 0.0F && ws.enemies_spawned < ws.total_to_spawn) {
        // Spawn one enemy from the current group
        ws.enemies_spawned++;
        ws.enemies_alive++;
        ws.spawn_timer = wd.spawn_interval;
    }

    // Check if wave is complete (all enemies spawned)
    if (ws.enemies_spawned >= ws.total_to_spawn && ws.enemies_alive <= 0) {
        ws.complete = true;
    }
}

void EncounterManager::check_triggers(EncounterState& state, float dt) {
    // Find the encounter def
    const EncounterDef* def = nullptr;
    for (const auto& d : defs_) {
        if (d.id == state.id) { def = &d; break; }
    }
    if (!def) return;

    for (int i = 0; i < static_cast<int>(state.triggers.size()); ++i) {
        auto& ts = state.triggers[i];
        if (ts.fired) continue;

        if (i < static_cast<int>(def->triggers.size())) {
            const auto& td = def->triggers[i];

            switch (td.condition) {
                case TriggerCondition::Timer:
                    ts.timer += dt;
                    if (ts.timer >= td.delay_seconds) {
                        ts.fired = true;
                    }
                    break;

                case TriggerCondition::PreviousDone: {
                    // Check if the referenced wave/objective is complete
                    bool prev_done = true;
                    if (!td.depends_on_id.empty()) {
                        for (const auto& ws : state.waves) {
                            if (ws.id == td.depends_on_id && !ws.complete) {
                                prev_done = false; break;
                            }
                        }
                        for (const auto& os : state.objectives) {
                            if (os.id == td.depends_on_id && !os.complete) {
                                prev_done = false; break;
                            }
                        }
                    }
                    if (prev_done) ts.fired = true;
                    break;
                }

                case TriggerCondition::AllEnemiesDead: {
                    bool all_dead = true;
                    for (const auto& ws : state.waves) {
                        if (ws.enemies_alive > 0) { all_dead = false; break; }
                    }
                    if (all_dead) ts.fired = true;
                    break;
                }

                default:
                    // PlayerEnter and EnemyDeath are handled externally
                    break;
            }
        }
    }
}

void EncounterManager::check_objectives(EncounterState& state) {
    const EncounterDef* def = nullptr;
    for (const auto& d : defs_) {
        if (d.id == state.id) { def = &d; break; }
    }
    if (!def) return;

    for (int i = 0; i < static_cast<int>(state.objectives.size()); ++i) {
        auto& os = state.objectives[i];
        if (os.complete) continue;

        if (i < static_cast<int>(def->objectives.size())) {
            const auto& od = def->objectives[i];

            switch (od.type) {
                case ObjectiveType::KillAll:
                    if (os.kill_count >= od.kill_target) {
                        os.complete = true;
                    }
                    break;

                case ObjectiveType::Survive:
                    os.elapsed += 0.0F; // updated externally
                    if (os.elapsed >= od.survive_duration) {
                        os.complete = true;
                    }
                    break;

                default:
                    break;
            }
        }
    }
}

void EncounterManager::notify_enemy_killed(const std::string& wave_id,
                                            ai::CombatArchetype archetype) {
    (void)archetype;
    for (auto& state : states_) {
        if (state.phase != EncounterPhase::Active) continue;

        // Decrement alive count in the matching wave
        for (auto& ws : state.waves) {
            if (ws.id == wave_id) {
                if (ws.enemies_alive > 0) ws.enemies_alive--;
            }
        }

        // Increment kill count for any kill objectives
        for (auto& os : state.objectives) {
            if (os.id.find("kill") != std::string::npos ||
                os.id.find("Kill") != std::string::npos) {
                os.kill_count++;
            }
        }
    }
}

void EncounterManager::check_player_volume(const std::string& encounter_id,
                                            float px, float py, float pz) {
    for (auto& state : states_) {
        if (state.id != encounter_id) continue;
        if (state.phase != EncounterPhase::Active) continue;

        const EncounterDef* def = nullptr;
        for (const auto& d : defs_) {
            if (d.id == encounter_id) { def = &d; break; }
        }
        if (!def) return;

        for (int i = 0; i < static_cast<int>(state.triggers.size()); ++i) {
            auto& ts = state.triggers[i];
            if (ts.fired) continue;

            if (i < static_cast<int>(def->triggers.size())) {
                const auto& td = def->triggers[i];
                if (td.condition != TriggerCondition::PlayerEnter) continue;

                if (px >= td.vol_min_x && px <= td.vol_max_x &&
                    pz >= td.vol_min_z && pz <= td.vol_max_z &&
                    py >= td.vol_min_y && py <= td.vol_max_y) {
                    ts.fired = true;

                    // Activate the triggered spawn wave or objective
                    if (!td.spawn_wave_id.empty()) {
                        for (int wi = 0; wi < static_cast<int>(state.waves.size()); ++wi) {
                            if (state.waves[wi].id == td.spawn_wave_id) {
                                start_wave(state, wi);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

const EncounterState* EncounterManager::get_state(const std::string& id) const {
    for (const auto& state : states_) {
        if (state.id == id) return &state;
    }
    return nullptr;
}

const SpawnWaveDef* EncounterManager::current_wave_def(const std::string& id) const {
    for (std::size_t si = 0; si < states_.size(); ++si) {
        if (states_[si].id != id) continue;
        if (si >= defs_.size()) return nullptr;
        int wi = states_[si].current_wave_index;
        if (wi >= 0 && wi < static_cast<int>(defs_[si].waves.size())) {
            return &defs_[si].waves[wi];
        }
        break;
    }
    return nullptr;
}

bool EncounterManager::encounter_origin(const std::string& id,
                                         float& out_x, float& out_z) const {
    for (const auto& def : defs_) {
        if (def.id == id) {
            out_x = def.origin_x;
            out_z = def.origin_z;
            return true;
        }
    }
    return false;
}

std::vector<std::string> EncounterManager::pending_objectives(const std::string& id) const {
    std::vector<std::string> result;
    for (const auto& state : states_) {
        if (state.id != id) continue;
        for (const auto& os : state.objectives) {
            if (!os.complete) result.push_back(os.id);
        }
        break;
    }
    return result;
}

}  // namespace ahamkara::game
