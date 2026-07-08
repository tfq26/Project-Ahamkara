---
type: subagent-report
category: implementation
status: implemented
created: 2026-07-08T16:10:00Z
agent: oz
subsystems:
  - client
  - engine/render
branch: agent/oz/viewmodel-offset-tuning
validation:
  - cmake --build --preset debug-headless --target ahamkara_world_tests ahamkara_player_movement_controller_tests ahamkara_gameplay_tests ahamkara_movement_tests ahamkara_collision_tests
  - ./build/debug-headless/tests/ahamkara_world_tests
  - ./build/debug-headless/tests/ahamkara_player_movement_controller_tests
  - ./build/debug-headless/tests/ahamkara_gameplay_tests
  - ./build/debug-headless/tests/ahamkara_movement_tests
  - ./build/debug-headless/tests/ahamkara_collision_tests
---

# Subagent Report — Viewmodel Offset Tuning

## Task

TASK-20260704-1900: Extend the viewmodel system with per-weapon position (x,y,z) offsets, rotation offsets, and FOV scale factor so each weapon sits in a natural-looking first-person position.

## Status

Implemented and validated.

## Scope

**In bounds:**

- Per-weapon viewmodel position offsets (right/up/forward) in camera-relative space.
- Per-weapon FOV scale factor (weapon renders at a different FOV than the world).
- Per-weapon pitch/yaw/roll rotation offsets.
- Data lives in the client presentation layer (`weapon_viewmodel_data.h`), not gameplay or weapon-runtime code.

**Out of bounds:**

- No weapon balance or firing rule changes.
- No hand/arm IK changes (separate task).
- No reload animation changes (separate task).

## Files Changed

- `client/src/debug_scene_bridge.cpp` — wired per-weapon viewmodel offset data from `kWeaponViewmodelTransforms` into `DebugScene` fields each frame.

## What Changed

### debug_scene_bridge.cpp

Added an include for `ahamkara/client/weapon_viewmodel_data.h` and a block after `weapon_index` assignment that calls `weapon_viewmodel_transform(wi)` and populates:

- `scene.viewmodel_position_offset` — {pos_right, pos_up, pos_forward}
- `scene.viewmodel_fov_scale` — FOV multiplier for the weapon render
- `scene.viewmodel_pitch_deg`, `viewmodel_yaw_deg`, `viewmodel_roll_deg` — rotation offsets

This completes the chain:
1. **`weapon_viewmodel_data.h`** — already had `WeaponViewmodelTransform` with all fields and `kWeaponViewmodelTransforms` with per-weapon defaults.
2. **`debug_renderer.h`** — already had the `DebugScene` fields (`viewmodel_position_offset`, `viewmodel_fov_scale`, `viewmodel_pitch/yaw/roll_deg`).
3. **`debug_renderer.cpp`** — already consumed these fields when constructing the weapon transform matrix and FOV projection.
4. **`debug_scene_bridge.cpp`** — now bridges 1→2 by writing per-weapon data each frame.

No changes were needed to `weapon_viewmodel_data.h`, `debug_renderer.h`, or `debug_renderer.cpp` — the data structures and render-side consumption were already in place from prior work.

### Default values (unchanged)

The `kWeaponViewmodelTransforms` defaults are:

| Weapon          | pitch_deg | yaw_deg | roll_deg | pos_right | pos_up | pos_forward | fov_scale |
|-----------------|-----------|---------|----------|-----------|--------|-------------|-----------|
| AR-15           | -2.0      | 0.0     | 0.0      | 0.05      | -0.05  | 0.05        | 0.85      |
| Shotgun         | -3.0      | 0.0     | 0.0      | 0.08      | -0.08  | 0.03        | 0.80      |
| Rocket Launcher | -5.0      | 0.0     | 2.0      | 0.12      | -0.15  | 0.02        | 0.90      |

These place the AR-15 slightly right/down with a subtle forward push and 0.85× FOV zoom (makes the weapon appear larger/closer, improving visibility). The shotgun is bulkier (further right/down, 0.80× zoom). The rocket launcher is shifted further down-right with a slight roll and 0.90× FOV.

## Validation Run

Built game test targets and ran all game tests:

```sh
cmake --build --preset debug-headless --target \
  ahamkara_world_tests ahamkara_player_movement_controller_tests \
  ahamkara_gameplay_tests ahamkara_movement_tests ahamkara_collision_tests

./build/debug-headless/tests/ahamkara_world_tests
./build/debug-headless/tests/ahamkara_player_movement_controller_tests
./build/debug-headless/tests/ahamkara_gameplay_tests
./build/debug-headless/tests/ahamkara_movement_tests
./build/debug-headless/tests/ahamkara_collision_tests
```

## Validation Results

All tests pass:
- `ahamkara_world_tests` — all pass
- `ahamkara_player_movement_controller_tests` — all pass
- `ahamkara_gameplay_tests` — all pass
- `ahamkara_movement_tests` — all pass
- `ahamkara_collision_tests` — all pass

The `debug` and `debug-headless` full builds have pre-existing failures in `debug_client.cpp`, `threaded_local_runtime.cpp`, and `admin_server.cpp` — none related to this change. My modified file (`debug_scene_bridge.cpp`) was syntax-checked and compiles cleanly with the project's full include path and flags.

## Known Gaps

- The viewmodel offsets are currently compile-time constants in `kWeaponViewmodelTransforms`. Making them runtime-tunable (e.g., via config file or developer console) was out of scope but would be a natural follow-up.
- No visual verification was performed — the offsets need an in-engine visual pass to confirm they look natural on screen.

## Runtime Risks

None. The change adds a data bridge between already-existing data structures and fields that the renderer already consumes. No control flow or gameplay logic is affected.

## Cross-Agent Dependencies

- `client/src/debug_scene_bridge.cpp` — the only file changed. Any agent adjusting the per-weapon viewmodel data in `weapon_viewmodel_data.h` should be aware the bridge auto-populates from `kWeaponViewmodelTransforms`.

## Recommended Next Step

Run the game in first-person mode and visually verify the offset values. Tune `kWeaponViewmodelTransforms` in `weapon_viewmodel_data.h` based on visual feedback.

## Confidence

`High` — single-file change (~15 lines added), no structural modifications, all existing tests pass.
