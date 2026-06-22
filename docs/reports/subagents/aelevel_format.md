# Handoff Report: First .aelevel Format

## Task

Build the first `.aelevel` importer/runtime format — task #2 from the immediate
recommended next actions in the Ahamkara integration plan.

## Why This Task First

After inspecting all 3 candidate tasks against actual codebase state:

- **Task 1 (netcode wiring):** SequenceTracker, SnapshotInterpolator, and
  ClientPredictionManager are already wired in the headless client (the main
  gap is a visual render-path connection). The `ae_network -> ahamkara_game`
  dependency reported by subagents is actually already fixed in code.

- **Task 3 (animation + material to render):** Requires extending
  RenderBackend with texture abstractions and bridging animation into the
  renderer — this touches `debug_renderer.cpp` (the highest-conflict file
  across multiple phases) and is high-risk.

- **Task 2 (.aelevel format):** Extends a proven, stable asset pipeline
  without touching the renderer, game loop, or runtime integration surfaces.
  Safe, additive, and unlocks content creation.

## What Was Done

### New Files Created

| File | Purpose |
|------|---------|
| `engine/render/include/ae/render/compiled_level.h` | `LevelAsset` struct, `CompiledLevelFormat`, `save_compiled_level()`, `CompiledLevelLoader` |
| `engine/render/src/compiled_level.cpp` | Binary save/load implementation with `write_bytes`/`read_bytes` patterns matching existing compiled formats |
| `tools/asset_importer_level.h` | `load_level_source()`, `compile_level()` declarations |
| `tools/asset_importer_level.cpp` | `.lvl` source parser (key=value + `[section]` format), compile helper |
| `assets/levels/javelin4.lvl` | Sample level source mirroring the 42 `kDebugMapColliders` |

### Files Modified

| File | Change |
|------|--------|
| `engine/render/CMakeLists.txt` | Added `src/compiled_level.cpp` to `ae_render` library |
| `tools/CMakeLists.txt` | Added `asset_importer_level.cpp` to importer executable |
| `tools/asset_importer_dispatch.h` | Added `compile_level()` declaration |
| `tools/asset_importer_dispatch.cpp` | Added `#include "asset_importer_level.h"`, `"level"` kind routing, updated usage text |
| `game/CMakeLists.txt` | Added `ae_render` PRIVATE link dep; added render include dir PUBLIC |
| `game/include/ahamkara/game/world.h` | Added `#include "ae/render/compiled_level.h"`, `owned_colliders_`, `load_colliders_from_level()` |
| `game/src/world.cpp` | Implemented `load_colliders_from_level()` — converts LevelAsset to ColliderBox + Jolt bodies + spawn |
| `client/include/ahamkara/client/local_play.h` | Added `load_level(path)` method |
| `client/src/local_play.cpp` | Implemented `load_level()` via `CompiledLevelLoader` |
| `client/src/main.cpp` | Parses `--level` flag, passes to dispatch functions |
| `client/src/headless_clients.cpp` | `run_sandbox_client` accepts `level_path`, loads level on startup |
| `client/src/debug_client.cpp` | `run_local_client` accepts `level_path`, `ThreadedSimulation` loads level |
| `server/src/dedicated_server_main.cpp` | Parses `--level` flag, loads level via `CompiledLevelLoader` on startup |
| `tests/CMakeLists.txt` | Added `ae_render` to `ahamkara_smoke_tests` link deps |
| `tests/src/asset_pipeline_tests.cpp` | Added 3 new tests: level roundtrip, bad-magic rejection, import roundtrip |
| `assets/manifest.assets` | Added level manifest entry |
| `docs/systems/asset_pipeline.md` | Added level kind to table, source format docs, runtime format list |

### .aelevel Binary Format (Version 1)

Magic: `0x5654454C` ("LEVEL"), version 1.

Sections in order:
1. **World settings:** name (uint32-prefixed), sky color (3 floats), ambient (3 floats), gravity (float), skybox/ground material asset IDs (strings)
2. **Spawn points:** uint32 count, then per-point (pos_x/y/z, yaw, team)
3. **Collision boxes:** uint32 count, then per-box (min_x/z, max_x/z, top_y, bottom_y, wall/jump_through/auto_step as uint32 flags, surface_material)
4. **Mesh instances:** uint32 count, then per-instance (mesh/material asset IDs as strings, position, yaw/pitch/roll, scale)

### .lvl Source Format

Top-level `key=value` lines for world settings, then `[spawn]`, `[collision]`, and `[mesh]` sections with space-separated positional values and optional `key=value` flags.

### Test Results

All 8 CTest targets pass (0 failures):

```
ahamkara_smoke_tests ............. Passed
ahamkara_world_tests ............. Passed
ahamkara_movement_tests .......... Passed
ahamkara_collision_tests ......... Passed
ahamkara_gameplay_tests .......... Passed
ahamkara_session_tests ........... Passed
ahamkara_utility_tests ........... Passed
ahamkara_asset_pipeline_tests .... Passed  (9 sub-tests incl. level roundtrip/bad-magic/import)
```

The level importer is idempotent (cache skip on second run). The server,
sandbox client, and debug view client all accept `--level <path>` and
fall back to hardcoded `kDebugMapColliders` if no level is specified.

## What Is NOT Done (Intentionally)

- **No level visual preview or editor:** Format is batch-import only.

- **No mesh instance rendering from .aelevel:** `LevelMeshInstance` data is loaded
  but no renderer path exists to place and draw mesh instances in the world.

- **Network client prediction world not synced to server's level:** The
  `ClientPredictionManager` creates a default World with hardcoded colliders.
  Reconciliation corrects prediction errors, but level-aware prediction would
  improve accuracy.

## Runtime Integration (Completed)

The `.aelevel` format is now wired into the runtime:

- **`World::load_colliders_from_level(LevelAsset)`** — converts `LevelCollisionBox`
  entries to game-side `ColliderBox`, rebuilds Jolt physics bodies, and sets
  player spawn from the first spawn point. Stores colliders in `owned_colliders_`
  vector so the raw pointer stored in `colliders_` remains valid.

- **`LocalPlaySimulation::load_level(path)`** — bridge that loads a compiled level
  and applies it to the local World for visual client (debug view) use.

- **Server `--level <path>` flag** — loads the `.aelevel` at startup. Falls back
  to hardcoded `kDebugMapColliders` if no level is specified or loading fails.

- **Sandbox client `--level <path>` flag** — same behavior via
  `run_sandbox_client(const char* level_path)`.

- **Debug view client `--level <path>` flag** — same behavior via
  `run_local_client(..., const char* level_path)`.

- **Game library dependency:** `ahamkara_game` now links `ae_render` PRIVATE
  for `CompiledLevelLoader` (compiled_level.cpp has no GL deps so headless
  server safety is preserved). `ae_render/include` is PUBLIC so consumers
  of `world.h` can resolve `LevelAsset` references.

All 8 CTest targets pass (0 failures) with these changes.

## Next Steps (Recommended)

1. **Continue with netcode wiring (original task #1)** — the headless client
   already has SequenceTracker/SnapshotInterpolator/ClientPredictionManager
   wired; the gap is connecting interpolated state to the visual debug client's
   render path.

2. **Add mesh instance rendering from .aelevel** — place `LevelMeshInstance`
   entries as draw calls in the renderer, loading `.aemesh` and `.aemat`
   assets by their asset IDs.

3. **Add a runtime manifest loader** that reads `asset_registry.tsv` and
   resolves asset IDs to file paths, enabling level mesh/material references
   to load actual compiled assets at runtime.

4. **Hook animation + materials/textures into a visible render path**
   (original task #3).

## Code Conventions Followed

- Binary I/O uses `ae::render::binary_io.h` helpers exclusively (no raw fstream)
- Magic + version header pattern matches `.aemesh`, `.aetex`, `.aemat`
- Loader class pattern with `last_error()` matches `CompiledMeshLoader` etc.
- `checked_count()` guards against overflow
- `validate_count()` guards against unreasonable element counts
- Tests follow the anonymous-namespace + fail() + temp-directory pattern
- Level asset lives in `ae::render` namespace (not game) — clean layering
- CTest integration via existing `ahamkara_asset_pipeline_tests` target
