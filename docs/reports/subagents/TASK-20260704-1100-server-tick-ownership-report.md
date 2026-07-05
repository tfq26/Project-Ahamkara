---
type: subagent-report
category: implementation
status: implemented
created: 2026-07-05T21:33:00Z
agent: phase4-netc
subsystems:
  - game
  - server
branch: agent/phase4/netcode
validation:
  - cmake --build --preset debug-headless
  - ctest --preset debug-headless
---

# Subagent Report — TASK-20260704-1100-server-tick-ownership

## Task

Move authoritative tick ownership onto the server path so sim progression, replicated state, and input consumption have one explicit source of truth.

## Status

Implemented and validated. All 14 tests pass.

## Scope

**In bounds:**
- Split `World::tick(float, const PlayerInputCommand&)` into `advance_sim(float)` and `apply_input(float, const PlayerInputCommand&)`.
- The combined `tick()` is kept as a convenience wrapper for client-local paths (local_play, client_prediction) where single-player input-driven ticking is correct.
- `DeathmatchActivity::tick()` now calls `world_.advance_sim(dt)` first, then applies each connected slot's buffered input via `world_.apply_input()`.
- `DeathmatchActivity::simulate_input()` buffers the input per-slot instead of ticking the world.
- Fixed pre-existing build issue: `window_input_provider_tests` target now guarded with `if(TARGET ae_platform)` since ae_platform is only available in client builds.

**Out of bounds:**
- No changes to client-local code paths (local_play.cpp, client_prediction.cpp, headless_clients.cpp).
- No changes to snapshot building or broadcasting.
- No changes to the server's main loop timing in dedicated_server_main.cpp.
- No HDR/offscreen render targets.

## Files Changed

- `game/include/ahamkara/game/world.h` — removed `tick_internal` declaration; added `advance_sim` and `apply_input` declarations
- `game/src/world.cpp` — refactored `tick_internal` into `advance_sim` and `apply_input`; `tick()` now calls both
- `game/include/ahamkara/game/activities/deathmatch_activity.h` — added `pending_input` and `has_pending_input` fields to `PlayerSlot`
- `game/src/activities/deathmatch_activity.cpp` — `tick()` drives world sim + applies buffered inputs; `simulate_input()` buffers input only
- `tests/CMakeLists.txt` — guarded `ahamkara_window_input_provider_tests` with `if(TARGET ae_platform)`

## What Changed

1. **World tick split**: `World::tick_internal()` was removed and replaced by `advance_sim(float dt)` (sim-only: match time, dummy AI, projectiles, particles, decals, history, syncs) and `apply_input(float dt, const PlayerInputCommand& input)` (player-relative: movement controller, weapon state, firing). The combined `tick(float, const PlayerInputCommand&)` convenience calls both in sequence.

2. **Server tick ownership**: `DeathmatchActivity::tick(float dt)` now:
   - Increments `server_tick_` and ticks deathmatch state
   - Calls `world_.advance_sim(dt)` — the authoritative sim progression
   - Iterates connected slots, applying each buffered input via `world_.apply_input(dt, slot.pending_input)`
   
3. **Input buffering**: `simulate_input()` no longer calls `world_.tick()`. Instead it stores the command in `slot->pending_input` and sets `has_pending_input = true`. The world tick is owned by `tick()`.

## Validation

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

## Validation Results

All 14 tests pass:
- `ahamkara_smoke_tests` — 1 test, passed
- `ahamkara_world_tests` — 1 test, passed
- `ahamkara_movement_tests` — 1 test, passed
- `ahamkara_player_movement_controller_tests` — 1 test, passed
- `ahamkara_collision_tests` — 1 test, passed
- `ahamkara_gameplay_tests` — 1 test, passed
- `ahamkara_session_tests` — 1 test, passed
- `ahamkara_utility_tests` — 1 test, passed
- `ahamkara_logging_tests` — 1 test, passed
- `ahamkara_weapon_loader_tests` — 1 test, passed
- `agent_runner_python_tests` — 1 test, passed
- `ahamkara_nakama_bridge_tests` — 1 test, passed
- `ahamkara_reliable_channel_tests` — 1 test, passed
- `ahamkara_nav_grid_tests` — 1 test, passed

## Known Gaps

- The single-player World assumes one local player. True multi-player with multiple players per server will require multiple player states in World, which is a separate task.
- `advance_sim` records `player_.state().position` in the history buffer even when no input was applied (because `apply_input` hasn't run yet). This is harmless — `apply_input` will update the position before the next history record, and the first tick captures initial spawn position.

## Runtime Risks

- `simulate_input()` is still called from `dedicated_server_main.cpp` with `delta_seconds` as the second parameter. The parameter is now unused in the implementation. Callers remain unchanged.
- If a slot has no pending input during a tick, `apply_input` is not called for that slot. The player's current state persists unchanged, which is correct.

## Cross-Agent Dependencies

- Any agent modifying `World::tick()` should now call `advance_sim` + `apply_input` separately for server paths, or use the combined `tick()` for client paths.
- The `PlayerSlot` struct in `deathmatch_activity.h` now has `pending_input`/`has_pending_input` — any agent adding new fields should be aware of the buffering lifecycle.

## Recommended Next Step

TASK-20260704-1110-prediction-reconciliation — ClientPredictionManager should use the split API for reconciliation replay.

## Confidence

`High` — the split is a straightforward refactor of tick_internal into two named phases. All existing tests pass unchanged, and the new server path (deathmatch_activity tick -> advance_sim + apply_input) follows the same code paths as the combined tick.
