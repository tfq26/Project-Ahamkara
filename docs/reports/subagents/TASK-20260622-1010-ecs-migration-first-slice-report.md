---
type: subagent-report
category: implementation
status: validated_with_known_gaps
created: 2026-06-23
agent: opencode
subsystems:
  - game
  - build
branch: main
validation:
  - "cmake --build --preset debug-headless (clean: all targets LINK, incl. world/gameplay/smoke + server + wish_test_client)"
  - "./scripts/run-tests.sh --preset debug-headless (10/10 pass)"
  - "cmake --build --preset debug (clean, incl. ahamkara_client)"
  - "ctest --test-dir build/debug --output-on-failure (12/12 pass)"
---

# Subagent Report

## Task

ECS migration first slice — second revision after Codex `revise`. Finish
migrating projectiles fully onto `entt::registry` by removing the fixed array
while **preserving the original public accessor shape**, and make the requested
`debug-headless` validation actually runnable. Task:
`TASK-20260622-1010-ecs-migration-first-slice`.

## Status

validated_with_known_gaps — both validation lanes now pass (debug-headless 10/10,
debug 12/12, tests unmodified). Remaining honest gap: the restored projectile
population is runtime-visible and not yet confirmed on a running display.

## Addressing The Codex Review (revise)

1. **"stable public accessors" / no accessor-shape change** — Reverted the
   `std::vector` return API. `get_projectiles()` is again
   `const ProjectileState*` and `get_projectile_count()` is again `int`, exactly
   the prior signatures. The fixed array is still removed: a **dynamic**
   `std::vector<ProjectileState> projectiles_` projection backs the pointer,
   refreshed each tick from the registry view (`sync_projectiles_to_array()`).
   No fixed-size entity array; registry stays authoritative; accessor shape
   unchanged. Consumer call sites reverted to their original form.
2. **debug-headless validation blocked by `ae_render`** — Fixed the
   pre-existing build gap. `ae_render` (GL/GLFW) is only built in non-headless
   configs, but `ahamkara_game` linked it `PRIVATE` and leaked `-lae_render`
   transitively into headless test exes. `ahamkara_game` already compiles the
   GL-free `compiled_level.cpp` itself and references no other render symbols, so
   the link is now guarded with `if(TARGET ae_render)` (same for the one explicit
   test link, `ahamkara_smoke_tests`). Headless now links and tests run.
3. **runtime-confirm-pending** — See Runtime Risks; mitigated by both suites now
   passing, but a display check is still recommended.

## Files Changed

- `game/include/ahamkara/game/world.h` — `get_projectiles()`/
  `get_projectile_count()` restored to pointer/int, backed by new dynamic member
  `std::vector<ProjectileState> projectiles_`; re-added
  `sync_projectiles_to_array()` decl; (fixed array + dead
  `set_projectile_count`/`projectiles_mut` remain removed from the prior pass).
- `game/src/world.cpp` — `sync_projectiles_to_array()` now refreshes
  `projectiles_` from `registry_.view<const WorldProjectileComponent>()`; called
  each tick alongside `sync_dummies_to_array()`.
- `game/CMakeLists.txt` — `ae_render` link guarded with `if(TARGET ae_render)`.
- `tests/CMakeLists.txt` — `ahamkara_smoke_tests` `ae_render` link guarded.
- `client/include/ahamkara/client/local_play.h`, `client/src/local_play.cpp`,
  `client/src/threaded_local_runtime.cpp`,
  `game/src/activities/deathmatch_activity.cpp` — reverted to the original
  pointer/count consumer shape.

## What Changed

The EnTT registry is the sole authoritative store for projectiles (no fixed
array); a per-tick dynamic projection preserves the existing pointer/count
accessor contract. The headless build no longer references a render library it
doesn't need, so the standard headless test command works.

## Validation Run

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure
```

## Validation Results

- debug-headless: clean build (all exes link) + **10/10 tests pass**.
- debug: clean build (incl. `ahamkara_client`) + **12/12 tests pass**.
- The 10 vs 12 difference is the two `if(TARGET ae_render)`-guarded render-only
  tests (`asset_pipeline_tests`, `level_render_tests`) that legitimately do not
  build headless. **No test files were modified.**

## Known Gaps

- Dummies remain on a fixed array + sync (out of scope; their pointer accessor
  is depended on by unmodified `world_tests`). Future migration would need a
  test-compatible accessor decision.

## Runtime Risks

- Projectile replication/render was silently dead before (count always 0 from
  the no-op sync); it is now populated from the registry. This is the intended
  completion of the migration but is runtime-visible. Headless/debug suites pass
  and exercise the projectile path (gameplay/smoke), but on-screen projectile
  rendering and snapshot payloads should get a quick runtime/display check.
- `sync_projectiles_to_array()` allocates within a reused vector per tick (≤64);
  negligible, optimize only if profiled.

## Cross-Agent Dependencies

- Build change: `ae_render` is now linked only when present. Any new headless
  target that genuinely needs render symbols must build/provide `ae_render`
  itself. Candidate `known-traps` note: render-linked test exes require a
  non-headless config (or the guard) to link.

## Recommended Next Step

Codex: re-review the projectile slice (accessor shape preserved, array removed,
both suites green). Optionally fold the `ae_render` headless-link fix into a note
in `known-traps`/build docs.

## Confidence

high — accessor contract preserved, fixed array removed with registry as sole
store, both presets build clean and pass all tests with no test edits; only the
runtime/display confirmation of restored projectile population remains.
