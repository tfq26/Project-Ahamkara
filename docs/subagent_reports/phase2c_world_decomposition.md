# Phase 2C: World Decomposition — Subagent Report

## Summary

Decomposed the monolithic `game/src/world.cpp` (1339 lines) into five focused units by extracting internal helper modules. The `World` class API remains unchanged; all decomposition uses internal headers in `game/src/` and thin wrapper methods.

## Before / After

| File | Before | After |
|------|--------|-------|
| `game/src/world.cpp` | 1339 lines | 679 lines |
| `game/src/world_jolt_bridge.cpp` | — | 166 lines |
| `game/src/world_dummy_sim.cpp` | — | 82 lines |
| `game/src/world_projectile.cpp` | — | 300 lines |
| `game/src/world_camera.cpp` | — | 114 lines |

**Reduction**: `world.cpp` shrank by **49%** (1339 → 679 lines).

## New Files

### `game/src/world_jolt_bridge.h` / `.cpp` — Collision Bridge
- **What it contains**: Jolt Physics infrastructure — layer definitions (`Layers`, `BroadPhaseLayers`), filter implementations (`ObjectLayerPairFilterImpl`, `BPLayerInterfaceImpl`, `ObjectVsBroadPhaseLayerFilterImpl`), `AhamkaraCharacterContactListener`, `JoltWorldImpl` struct, `initialize_jolt_once()`, and `rebuild_jolt_colliders()` free function.
- **Rationale**: Isolates all Jolt-specific setup from the rest of the World logic. The `JoltWorldImpl` and contact listener were previously defined inline in `world.cpp`.

### `game/src/world_dummy_sim.h` / `.cpp` — Dummy Simulation
- **What it contains**: `TargetDummyComponent` (ECS component), `tick_dummies()` (respawn, scripted movement, hit feedback), and `sync_dummies_to_jolt()` (sync state to Jolt kinematic bodies).
- **Rationale**: Extracts ~50 lines of dummy update logic from `tick_internal()` into a self-contained unit with clear parameter interfaces.

### `game/src/world_projectile.h` / `.cpp` — Projectile / Combat
- **What it contains**: `ProjectileComponent` (ECS component), `BroadPhaseLayerFilterAll`, `ObjectLayerFilterAll`, `fire_projectile()` (spawn with aim-assist magnetism), and `step_projectiles()` (movement, raycast hit detection, rollback compensation).
- **Rationale**: The largest extracted block (~260 lines of projectile physics). These functions access many `World` private members, so they are declared as `friend` functions in `world.h`.

### `game/src/world_camera.h` / `.cpp` — Camera / Debug State
- **What it contains**: `has_move_input()` (shared helper), `update_camera_state()` (position, head bob, yaw/pitch), `resolve_movement_state()` (enum resolution), and `fill_movement_debug()` (debug visualisation data).
- **Rationale**: Extracts camera and debug logic into pure functions taking explicit parameters — no friend access needed.

## Changes to `world.h`

Minimal changes:
- Added two `friend` declarations for the projectile free functions:
  ```cpp
  friend void fire_projectile(World&, const PlayerInputCommand&);
  friend void step_projectiles(World&, float);
  ```

## Changes to `CMakeLists.txt`

Added four new source files to the `ahamkara_game` static library target.

## What stays in `world.cpp`

- Movement constants (`kGroundHeight`, `kJumpSpeed`, `kGravity`, etc.)
- `World::World()` constructor (Jolt KCC shape setup, dummy initialisation)
- `World::tick()` / `tick_internal()` (fixed-step loop, now calls helpers for dummies, projectiles, camera, and movement debug)
- `World::set_player_state()`, `is_on_ground()`, `get_player_visual_height()`
- `World::set_colliders()` / `recreate_jolt_colliders()` (thin wrappers)
- `World::resolve_mantle()`, `resolve_moving_platform()`, `resolve_ladder_and_ledge()` (remain inline — each under 50 lines and tightly coupled to Jolt KCC state)
- `World::spawn_damage_number()`, `queue_audio_event()`, `flush_audio_events()`
- `World::get_historical_state()`
- `World::spawn_projectile()` / `update_projectiles()` / `update_camera()` / `update_movement_state()` / `populate_movement_debug()` — all now thin wrappers delegating to helpers

## Validation

- **ahamkara_game** static library builds successfully (157 targets, no errors).
- **11/11 world tests pass**: `test_world_initialization`, `test_world_tick_movement`, `test_world_tick_rotation`, `test_world_camera_yaw_wraps`, `test_world_camera_pitch_clamped`, `test_world_platform_standing`, `test_world_platform_walking_off`, `test_world_wall_collision`, `test_world_jump_through`, `test_bullet_magnetism`, `test_rollback_lag_compensation`.
- **14/14 movement tests pass**: No regression in the separate movement module.
- Behavior is preserved — all extracted code is a pure cut-and-paste of the original logic.
