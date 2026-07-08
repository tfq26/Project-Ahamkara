# Report: TASK-20260704-1520-encounter-scripting

**Status:** Self-validated (batched for codex review)
**Date:** 2026-07-05
**Agent:** Oz (phase8-content)

## Files Changed

| File | Change |
|------|--------|
| `game/include/ahamkara/game/encounter_scripting.h` | **NEW** - `ObjectiveDef`/`ObjectiveType` (KillAll/Survive/Defend/Collect/Reach/Boss), `TriggerDef`/`TriggerCondition` (PlayerEnter/EnemyDeath/Timer/PreviousDone/AllEnemiesDead), `SpawnWaveDef`/`SpawnGroupDef` (enemy group definitions with archetype/count/spread), `EncounterDef` (full authored encounter), `ObjectiveState`/`SpawnWaveState`/`TriggerState`/`EncounterState` (runtime state), `EncounterManager` (complete lifecycle management). |
| `game/src/encounter_scripting.cpp` | **NEW** - Full EncounterManager implementation: add_encounter, start_encounter, start_all, clear, tick (wave spawning, trigger evaluation, objective checking, completion detection), notify_enemy_killed, check_player_volume, accessors for state/wave def/origin/pending objectives. |
| `game/CMakeLists.txt` | **MODIFIED** - Added `encounter_scripting.cpp` to ahamkara_game sources. |
| `tests/src/encounter_scripting_tests.cpp` | **NEW** - 9 tests: add encounter, start encounter, start nonexistent, wave spawn tick, simultaneous wave, objectives, timer trigger, enemy killed notification, clear. |

## Commands Run

```sh
cmake --build --preset debug
ctest --output-on-failure
```

## Test Results

All 19/19 tests pass. New ahamkara_encounter_scripting_tests: 9/9.

## Assumptions

- EncounterManager is a standalone runtime type, not yet wired into World::tick_internal (deferred to integration slice).
- Spawn wave spawning just increments counters — actual enemy entity creation is handled by the caller (World/Activity) using the wave def.
- Player volume triggers need external caller to invoke check_player_volume with player position each tick.
- The objective kill-count heuristic (substring matching "kill"/"Kill" in ID) is a placeholder; a proper objective binding system is deferred.

## Risks

- No runtime GL display confirmation.
- The encounter system is not yet integrated with the World tick or activity framework — that's a separate integration slice.
- Spawn position logic (distribute enemies around encounter origin with spread) is not implemented yet — currently just increments counters.
