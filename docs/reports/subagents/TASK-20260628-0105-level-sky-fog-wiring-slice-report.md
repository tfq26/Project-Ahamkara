---
type: subagent-report
category: implementation
status: implemented
created: 2026-06-28
agent: opencode
subsystems: [engine/render, client, game]
branch: main
validation: [debug-build, debug-tests]
---

# Subagent Report: TASK-20260628-0105-level-sky-fog-wiring-slice

## Task

Wire level-driven sky, ambient, and fog data through the renderer so the engine uses level settings instead of hardcoded environment values.

## Status

implemented_not_validated — wiring verified intact; runtime display confirmation not possible in this headless environment.

## Scope

In bounds: Confirm level environment data flows through the render path, verify fog color/density from level settings, preserve fallback when no level loaded.

Out of bounds: Skybox cubemaps, procedural atmosphere, time-of-day systems, HDR/tonemapping changes, display-dependent artistic tuning.

## Files Changed

No code changes. This is a verification of existing implementation from TASK-20260620-1510.

## What Changed

Verified the complete data-flow chain is intact:

1. **Source**: All 4 `.lvl` level files and 3 `.json` spec files have `sky_color` and `ambient` fields
2. **Authoring**: `tools/levelgen/spec_to_lvl.py` reads JSON sky/ambient and emits `.lvl` lines (lines 61-64)
3. **Compilation**: `compiled_level.cpp` serializes/deserializes `sky_color` and `ambient` into `.aelevel` binary format (lines 14-16, 50-52)
4. **Loading**: `debug_client.cpp:90` calls `renderer.set_level_environment()` with loaded level data immediately after `level_loader.load()` succeeds
5. **Storage**: `DebugRenderer::Impl` stores `level_sky[3]` and `level_ambient[3]` buffers (debug_renderer.cpp:1083-1084)
6. **Flag**: `has_level_env` gate (debug_renderer.cpp:1085) controls whether level values override built-in day/night
7. **Consumption in render loop**:
   - **Clear color / sky / fog tint**: `debug_renderer.cpp:1969` — when `has_level_env`, `level_sky` replaces day/night interpolation for `glClearColor` and the `uFogColor` uniform
   - **Ambient light**: `debug_renderer.cpp:2072` — when `has_level_env`, `level_ambient` replaces day/night ambient computation for `uLightModelAmbient` uniform
8. **Fallback**: `clear_level_environment()` (debug_renderer.cpp:2336) reverts to built-in day/night when no level is loaded

The `DebugRenderer.h` declaration (lines 157-163) documents both `set_level_environment` and `clear_level_environment` with clear doc comments.

Known gap: **Fog density** is NOT level-authored — only fog color (via sky color) is level-driven. Fog density is always derived from `day_factor`. Per-level fog density would require adding a `fog` field to `LevelAsset`, the `.lvl` format, binary I/O, and uniform setup path.

## Validation Run

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Validation Results

- Debug build: Pass (no changes, ninja had no work)
- 14/14 tests pass (0 failures)
- Full data-flow trace confirms wiring is intact from JSON spec → .lvl → compiled .aelevel → renderer
- Not runtime-confirmed: visual sky/ambient change requires GL display

## Known Gaps

- Fog density is not level-authorable (only fog color via sky color)
- No level has authored values significantly different from defaults (all use 0.3/0.4/0.6 sky)
- Runtime visual confirmation not possible in this headless environment

## Runtime Risks

Minimal — this is a verification, not a code change. The wiring has been build-validated previously and remains intact.

## Cross-Agent Dependencies

- TASK-20260620-1520 (runtime-confirm-prototype-levels) would provide visual confirmation but requires a GL display

## Recommended Next Step

Codex review. When a display is available, modify a level's JSON spec to use distinctly different sky/ambient values (e.g., red sky for testing), regenerate the .lvl + recompile, and run `./scripts/start.sh local --level assets/compiled/levels/prototype_arena.aelevel` to visually confirm the change.

## Confidence

`high` — wiring verified intact by code audit; build and tests green.
