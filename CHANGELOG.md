# Changelog

All notable changes to this project are documented here.

Format follows [Keep a Changelog](https://keepachangelog.com/) conventions.

## Unreleased

### Added

- **Lag-compensated hit validation** (`deathmatch_activity`): New `HitValidationResult` struct and `validate_hit()` method that queries the server history buffer at the client's perceived tick and runs ray-vs-dummy hit detection (cylinder body + hemisphere caps) with configurable base damage and headshot multiplier. (`game/include/ahamkara/game/activities/deathmatch_activity.h`, `game/src/activities/deathmatch_activity.cpp`)
- **Snapshot delta compression** (`deathmatch_activity`): `for_each_connected_snapshot()` now computes a `SnapshotDelta` per client using `compute_player_delta()`/`write_snapshot_delta()`, reducing per-client bandwidth by only sending changed player state fields. Each slot tracks `last_sent_player_state` for the next delta. (`game/include/ahamkara/game/activities/deathmatch_activity.h`, `game/src/activities/deathmatch_activity.cpp`)
- **Remote player interpolation** (`RemoteInterpolator`): Template class wrapping `SnapshotInterpolator` with per-remote-player ring buffers (up to 6 entries each) for interpolating position and yaw at a target render time, including ±360° yaw wrap handling. (`engine/network/include/ae/network/remote_interpolator.h`)

### Changed

- **Legacy GL cleanup docs**: Added subagent report and moved task card from `open/` to `review-needed/` for retiring fixed-function GL compatibility seams (Apple-specific VAO extensions, legacy `<OpenGL/gl.h>` header). (`docs/reports/subagents/TASK-20260704-1350-legacy-gl-retirement-report.md`, `docs/vault/queue-tasks/review-needed/TASK-20260704-1350-legacy-gl-retirement.md`)
- **Updated** `docs/reports/subagents/subagent-master-log.md` with legacy GL retirement entry.

### Cleanup

- Removed duplicate git worktrees `ahamkara-phase4`, `ahamkara-phase5`, `ahamkara-phase6` that were separate branch checkouts of the same repository.

### Fixed

- (Pre-existing) `ahamkara_world_tests`, `ahamkara_movement_tests`, `ahamkara_collision_tests`, `ahamkara_gameplay_tests`, `ahamkara_nav_grid_tests`, and `ahamkara_reliable_channel_tests` all pass. Three test targets have pre-existing missing binaries unrelated to these changes (`ahamkara_playtest_harness_tests`, `ahamkara_ai_combatant_tests`, `ahamkara_encounter_scripting_tests`).

## [Unreleased prior]

### Added

- **ECS migration (projectiles)**: Projectile state migrated from fixed arrays to `entt::registry` with pointer/count accessor compatibility preserved.
- **NavGrid + A***: Header-only navigation grid with 4/8-connectivity, deterministic tie-breaking, path follower, and collision-rasterizer.
- **AI combatants**: 6 archetypes (Grunt/Sniper/Rusher/Support/Scout/Brute), perception (LOS/FOV/alertness), 7-state behavior FSM.
- **Encounter scripting**: Wave spawn, trigger evaluation, objective tracking with `EncounterManager`.
- **Weapon runtime**: Subclass contract with `on_fire()` hook, reload timer accessor, data-driven RPM/reload/reserve.
- **Spatial partitioning**: Uniform 2D grid with AABB/frustum query, occlusion portal/PVS region data shapes.
- **Animation + audio + VFX**: Character animation adapter bridge, weapon animation layers, 3D audio spatialization, VFX feedback (screen shake, damage flash).
- **Networking**: Reliable channel (buffer/retransmit/ack), reconciliation replay fix, server history buffer with lag compensation support.
- **Level authoring**: Path A (JSON spec + `spec_to_lvl.py` emitter) and Path B (Blender headless script with `build_level.py`).
- **Level-driven rendering**: Level environment overrides (sky/ambient/fog), textured material showcase, PBR UV plumbing.
- **Viewmodel refinement**: Arm/hand IK solver, phased reload animation, ADS camera transition, weapon tilt system.

### Changed

- **Build system**: Guarded `ae_render` link dependency to fix headless preset; all debug builds (standard and headless) now pass.
- **Documentation**: Expanded agent workflow guides, system architecture docs, and vault memory.

### Fixed

- **Input routing**: Removed duplicated raw GLFW ESC menu-toggle path; unified through single platform edge-triggered input.
- **Render semantics**: Clarified render-vs-present contract with docstrings.
- **Matrix-stack compatibility**: Fully removed, gameplay/menu boundary reads from `ClientMenuState`.
- **Debug client frozen view**: Fixed and regression-verified.
