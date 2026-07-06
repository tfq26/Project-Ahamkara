---
type: subagent-report
category: implementation
status: implemented_not_validated
created: 2026-07-06
agent: opencode
subsystems: [client, engine/animation]
branch: agent/viewmodel/1930-reload
validation: [debug-build, debug-tests]
---

# Subagent Report: TASK-20260704-1930-reload-animation-sequence

## Task

Add a viewmodel reload animation sequence with visible phases (grab_mag → remove_mag → insert_mag → return_to_grip), driven by `WeaponRuntime::reload_timer()` duration, extending `WeaponAnimationController` with per-weapon reload timing data.

## Status

`implemented_not_validated` — GLFW3 is not available in this build environment (headless remote agent), and the `debug-headless` preset excludes the client code where all changes reside. The `debug` preset requires GLFW which is not installed. Pre-existing POSIX compatibility issues in the headless build block full validation.

## Scope

### In bounds
- Reload animation phases defined: GrabMag, RemoveMag, InsertMag, ReturnToGrip
- Each phase maps to a portion of the weapon's `reload_duration` via data-driven timing fractions
- Hand IK targets animate through each phase (hand moves to magazine position, back to grip)
- Weapon tilt/rotation during reload (weapon pivots to expose magwell)
- Per-weapon reload data (AR-15, Shotgun, Rocket Launcher)
- Phase timing is data-driven via per-weapon config
- Everything stays in the presentation layer

### Out of bounds
- No changes to weapon ammo/reload gameplay logic in `WeaponRuntime`
- No multi-player specific networking changes
- No new mesh or art asset creation
- No weapon balance changes

## Files Changed

- `client/include/ahamkara/client/weapon_viewmodel_data.h` — Added `ReloadPhase` enum, `WeaponReloadData` struct, `kWeaponReloadData` per-weapon array, `weapon_reload_data()` accessor
- `client/include/ahamkara/client/weapon_animation_controller.h` — Extended `WeaponAnimProfile` with `WeaponReloadData` member; added `reload_phase()`, `reload_ik_offset()`, `reload_normalized()` accessors; private tracking fields
- `client/src/weapon_animation_controller.cpp` — Replaced single-sine reload with phase-driven animation; added smoothstep helper; per-weapon reload data in profile factories; IK offset computation per phase; weapon tilt per phase
- `client/include/ahamkara/client/weapon_presentation.h` — Added optional `ik_offset` parameter to `apply_viewmodel_arm_ik()`
- `client/src/weapon_presentation.cpp` — Applied IK offset to grip socket target during reload animation
- `client/src/client_frame_pipeline.cpp` — Pass reload IK offset from animation controller to IK solver in `stage_build_scene()`

## What Changed

### 1. Data structures (`weapon_viewmodel_data.h`)

New types:
- `ReloadPhase` enum: `Idle`, `GrabMag`, `RemoveMag`, `InsertMag`, `ReturnToGrip`
- `WeaponReloadData` struct: phase timing fractions [0,1], magazine position (x/y/z), weapon tilt angles (pitch/yaw/roll degrees), position offsets (right/up/forward meters)
- `kWeaponReloadData[3]` per-weapon array with distinct profiles for AR-15, Shotgun, Rocket Launcher
- `weapon_reload_data()` accessor function

### 2. Phase-driven reload animation (`weapon_animation_controller.cpp`)

The reload timer drives normalized progress `[0, 1]`. Each phase is a range:
- **GrabMag [0.00–0.18/0.25/0.15]**: Hand IK target moves from grip socket to magazine position via smoothstep. Weapon tilt ramps up.
- **RemoveMag [0.18–0.45/0.55/0.45]**: Hand holds at magazine position. Weapon at full tilt (magwell exposed).
- **InsertMag [0.45–0.75/0.82/0.75]**: Hand holds at magazine position. Weapon tilt begins returning.
- **ReturnToGrip [0.75–1.00/0.82–1.00/0.75–1.00]**: Hand IK target moves back to grip socket. Weapon tilt fades to zero.

Phase timing is per-weapon data-driven (fractions differ for AR-15, Shotgun, Rocket Launcher).

### 3. IK offset integration (`weapon_presentation.cpp`, `client_frame_pipeline.cpp`)

- `apply_viewmodel_arm_ik()` accepts an optional `const float* ik_offset` parameter. When non-null, the offset is added to the grip socket position before IK solving.
- Pipeline computes offset from `weapon_animation_.reload_ik_offset()` and passes it to the IK solver.

### 4. Per-weapon profiles

- **AR-15** (2.0s): Moderate timing, mild tilt (-18° pitch, 8° yaw, -5° roll). Mag position slightly below/forward.
- **Shotgun** (3.5s): Longer phases, dramatic tilt (-25° pitch, 12° yaw, -8° roll). Mag further forward/lower.
- **Rocket Launcher** (2.8s): Quick grab, moderate tilt (-10° pitch, 5° yaw, -3° roll). Mag at tube rear.

## Phase Timing Table

| Phase | AR-15 | Shotgun | Rocket Launcher |
|---|---|---|---|
| Grab start | 0.00 | 0.00 | 0.00 |
| Grab end | 0.18 | 0.25 | 0.15 |
| Remove end | 0.45 | 0.55 | 0.45 |
| Insert end | 0.75 | 0.82 | 0.75 |
| Return end | 1.00 | 1.00 | 1.00 |

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

### Results

Validation was not fully performed. The `debug` preset fails during CMake configure because GLFW3 is not available in this headless environment. The `debug-headless` preset excludes all client code (where all changes live) but also fails with pre-existing POSIX compatibility issues (missing `arpa/inet.h`, `sys/resource.h`, `std::aligned_alloc` on MSVC).

Code changes follow existing patterns:
- Uses same matrix/rotation types (`ae::render::Mat4`) and helpers (`rotation_quat`, `translation`) as existing code
- Follows the same namespace pattern (`ahamkara::client`)
- Data-driven approach mirrors `kWeaponViewmodelTransforms` and `kWeaponGripSockets`
- IK offset parameter is optional (default `nullptr`) — zero-cost for non-reloading states

## Known Gaps

- Magazine position values are estimated (below grip area) and will need visual tuning once runtime display is available.
- The left hand grip socket is defined but not IK-solved during reload (the viewmodel_arms skeleton has only one arm chain).
- Weapon tilt values are initial estimates and will need tuning based on visual feedback.
- No runtime visual confirmation (no GL display in this environment).
- Validation (build + tests) could not be run for the client code.

## Runtime Risks

- If the reload timer from `WeaponRuntime` doesn't exactly match the animation controller's timer, the animation may desync (currently the controller manages its own `reload_timer_` based on `profile.reload_duration`, matching `WeaponRuntime::reload_timer()`).
- Magazine position offsets assume the weapon attach bone location; actual weapon models may differ.
- The phase transition at `reload_timer_ <= 0.0F` may occur slightly before the `ReturnToGrip` phase completes at `normalized == 1.0`. This is mitigated by the final phase overlapping the timer end.

## Cross-Agent Dependencies

- Grip socket values may need updates once actual weapon viewmodel meshes are in use.
- The left-arm IK chain will need reload animation when dual-arm rendering is added.

## Recommended Next Step

- Verify build + tests in an environment with GLFW available
- Runtime visual confirmation (requires GL display) to tune magazine positions and tilt angles
- Add reload animation to the animation driver's `AnimGameplayInput` for third-person consistency

## Confidence

`medium` — the code follows established patterns and the data-driven approach is robust, but build + runtime validation could not be performed in this environment.
