# Report: TASK-20260704-1510-ai-combatants

**Status:** Self-validated (batched for codex review)
**Date:** 2026-07-05
**Agent:** Oz (phase8-content)

## Files Changed

| File | Change |
|------|--------|
| `game/include/ahamkara/game/ai/ai_combatant.h` | **NEW** - `CombatArchetype` enum (Grunt/Sniper/Rusher/Support/Scout/Brute), `ArchetypeConfig` with per-archetype defaults, `PerceptionState` (LOS/FOV/alertness/falloff), `BehaviorState` machine (Idle/Patrol/Alert/Engage/Flank/Retreat/Investigate), `AICombatantComponent` (full ECS component), and system function declarations. |
| `game/src/ai_combatant.cpp` | **NEW** - Implementations: `angle_diff_deg` (signed angle wrapping), `los_clear_2d` (wall/LOS against ColliderBox list), `update_perception` (distance+FOV+LOS composite check with alertness ramp/decay), `update_targeting` (player targeting), `tick_behavior` (7-state FSM with distance/health/time-based transitions), `tick_ai_combatants` (ECS system entry point). |
| `game/CMakeLists.txt` | **MODIFIED** - Added `ai_combatant.cpp` to ahamkara_game sources. |
| `tests/src/ai_combatant_tests.cpp` | **NEW** - 11 tests: archetype configs x4 (grunt/sniper/rusher/brute), angle math x4 (same/positive/negative/wrap), LOS x2 (clear/blocked), archetype application. |

## Commands Run

```sh
cmake --build --preset debug
ctest --output-on-failure
```

## Test Results

All 19/19 tests pass. New ahamkara_ai_combatant_tests: 11/11.

## Assumptions

- Targeting is player-only (single player). Multi-target support is deferred.
- Behavior FSM is a first pass — tuning values (timers, thresholds) may need adjustment during gameplay.
- Combatant fire events and damage application need to be wired into the World's damage pipeline (deferred).
- NavAgent pathfinding integration is declared but not yet connected in tick_ai_combatants.

## Risks

- No runtime GL display confirmation.
- Behavior state machine transitions are logic-level only — actual movement commands (NavAgent, steering) not yet wired.
- Damage output values are placeholders from archetype configs; balance tuning deferred.
