---
type: subagent-report
category: implementation
status: implemented
created: 2026-07-05T22:05:00Z
agent: phase4-netc
subsystems:
  - game
  - client
branch: agent/phase4/netcode
validation:
  - cmake --build --preset debug-headless
  - ctest --preset debug-headless
---

# Subagent Report — TASK-20260704-1110-prediction-reconciliation

## Task

Make client-side prediction and reconciliation explicit and replayable so buffered input handling stays deterministic under latency.

## Status

Implemented and validated. All 14 tests pass.

## Scope

**In bounds:**
- ClientPredictionManager now owns its fixed timestep (default 1/60), no longer takes `delta_seconds` from the caller.
- `apply_input()` uses `advance_sim` + `apply_input` split API (matching server path).
- Prediction world runs in server mode (`is_client_ = false`) so dummies/AI are simulated identically to the server.
- Reconciliation replay uses the manager's fixed step, not a hardcoded value.
- Added `set_fixed_step()`, `fixed_step()`, and `prediction_tick()` accessors.

**Out of bounds:**
- No changes to local_play.cpp (already owns its own FixedTimestepAccumulator).
- No changes to the server path.
- No changes to snapshot building or interpolation.
- No HDR/offscreen render targets.

## Files Changed

- `game/include/ahamkara/game/client_prediction.h` — added `kDefaultFixedStep`, `set_fixed_step()`, `fixed_step()`, `prediction_tick()`; changed `apply_input` signature to single-arg
- `game/src/client_prediction.cpp` — uses split API, server-mode world, configurable step for replay
- `client/src/headless_clients.cpp` — updated `apply_input` call (removed second arg)
- `tests/src/world_tests.cpp` — updated `apply_input` calls and expected world mode in `test_first_snapshot_reconciliation`

## What Changed

1. **Internal fixed timestep**: `ClientPredictionManager` now stores `fixed_step_seconds_` (default 1/60). `apply_input()` no longer accepts `delta_seconds` — the manager uses its own step to ensure deterministic playback.

2. **Split API**: `apply_input()` calls `world_->advance_sim(fixed_step_)` then `world_->apply_input(fixed_step_, input)`, mirroring the server-authoritative path in DeathmatchActivity.

3. **Server-mode prediction**: The prediction world runs with `is_client_ = false`, enabling dummy AI simulation. This keeps the predicted state consistent with what the server will produce.

4. **Configurable replay**: `reconcile()` replay uses `fixed_step_seconds_` instead of `constexpr float kFixedStep = 1.0F / 60.0F`, allowing the step to be adjusted to match the server's tick rate via `set_fixed_step()`.

## Validation

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

All 14 tests pass, including `test_first_snapshot_reconciliation` which exercises the reconciliation/replay path.

## Known Gaps

- The prediction world runs all server-side sim (dummies, AI, projectiles). This is correct but may produce extra particles/decals in the prediction world that are never rendered.
- `prediction_tick_` is incremented per `apply_input()` call but is not yet synced to the server's tick counter. Future work should align them.

## Runtime Risks

- The prediction world now simulates dummies/AI (via `!is_client_` check), making prediction more accurate but also slightly more CPU-intensive per input tick. This is bounded by `kMaxPendingInputs` (128).
- `test_first_snapshot_reconciliation` was updated to use server-mode world for the expected result. Existing test invariants are preserved.

## Cross-Agent Dependencies

- Any agent using `ClientPredictionManager::apply_input()` must now pass one argument instead of two.
- `set_fixed_step()` should be called if the server uses a non-60Hz tick rate.

## Recommended Next Step

TASK-20260704-1120-snapshot-interpolation-lag-compensation — remote snapshot interpolation, state delta compression, server rewind.

## Confidence

`High` — the changes are a clean evolution of the TASK-1100 split API into the prediction path, and the existing reconciliation test validates the replay behavior.
