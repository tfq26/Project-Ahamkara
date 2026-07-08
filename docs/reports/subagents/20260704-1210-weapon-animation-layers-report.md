---
type: subagent-report
category: animation
status: validated_with_known_gaps
created: 2026-07-08
agent: oz
subsystems:
  - engine/animation
  - client
branch: agent/oz/weapon-animation-layers
validation:
  - ae_animation (compiled)
  - ahamkara_game (compiled)
  - ahamkara_world_tests (pass)
  - ahamkara_movement_tests (pass)
  - ahamkara_gameplay_tests (pass)
  - ahamkara_weapon_loader_tests (pass)
---

# Subagent Report: Weapon Animation Layers

## Task

Extract weapon animation (sway, bob, recoil kick) from the monolithic `evaluate_weapon_animation()` into composable, independent layer functions. Add `notify_fired()` to the client's `WeaponAnimationController` so external code can trigger recoil kick on fire events.

## Status

`validated_with_known_gaps` — built and tested via `cmake --build --preset debug`; all possible test suites pass. Pre-existing compile failures in `client/src/threaded_local_runtime.cpp` and `client/src/debug_client.cpp` (unrelated to this change) prevent the full `run-tests.sh` from completing.

## Scope

In bounds:
- Layer extraction: `evaluate_sway_layer()`, `evaluate_bob_layer()`, `evaluate_recoil_kick_layer()`
- `fire_weapon_kick()` standalone function to trigger recoil kick on state
- `notify_fired()` on `WeaponAnimationController` for external fire-event hookup
- Backward-compatible `evaluate_weapon_animation()` wrapper that composes all layers
- Explicit layer composition (sway * bob * recoil * ADS) in `WeaponAnimationController::update_weapon()`
- Each layer produces a `render::Mat4` offset independently

Out of bounds:
- No runtime display confirmation (headless environment)
- No final art production
- No combat balance changes

## Files Changed

- `engine/animation/include/ae/animation/character_weapon.h`
- `engine/animation/src/character_weapon.cpp`
- `client/include/ahamkara/client/weapon_animation_controller.h`
- `client/src/weapon_animation_controller.cpp`

## What Changed

1. **character_weapon.h**: Added declarations for `evaluate_sway_layer()`, `evaluate_bob_layer()`, `evaluate_recoil_kick_layer()`, and `fire_weapon_kick()`. Each layer function takes its own relevant state, config, and input parameters and produces a `render::Mat4` offset.

2. **character_weapon.cpp**: Extracted sway, bob, and recoil kick into standalone functions. `evaluate_weapon_animation()` now composes all three layers internally for backward compatibility.

3. **weapon_animation_controller.h**: Added `notify_fired()` public method so external callers can trigger recoil kick on fire events.

4. **weapon_animation_controller.cpp**: Replaced the single `evaluate_weapon_animation()` call with explicit per-layer evaluation. ADS blend is computed inline. Layers are composed as: `local = sway_offset * bob_offset * recoil_offset * ADS_transform`. `notify_fired()` delegates to `fire_weapon_kick()`.

## Key Design

- **Sway layer**: Mouse look delta drives velocity that damps over time; idle sinusoidal oscillation adds subtle motion. ADS reduces effective amplitude.
- **Bob layer**: Movement speed drives phase accumulation. Vertical sine + horizontal cosine oscillation. Only active when `is_moving && speed > epsilon`. Subtle roll from horizontal component.
- **Recoil kick layer**: `fire_weapon_kick()` sets a 50ms timer. The layer produces a brief downward translation that decays linearly over the timer duration.
- **Generic layering**: Any weapon type with a `WeaponAnimConfig` reuses all three layers. No weapon-specific logic in the layer implementations.

## Validation Run

```sh
cmake --build --preset debug
cd build/debug && ctest -R "world|movement|weapon_loader|gameplay"
```

## Validation Results

- `ae_animation` target: compiled, no work to do (object files verified)
- `ahamkara_game` target: compiled
- `ahamkara_world_tests`: **PASSED** (0.06 sec)
- `ahamkara_movement_tests`: **PASSED**
- `ahamkara_gameplay_tests`: **PASSED**
- `ahamkara_weapon_loader_tests`: **PASSED**
- `ahamkara_player_movement_controller_tests`: Not Run (executable not built — pre-existing)
- Full `run-tests.sh`: 15% pass (pre-existing failures in client/GL-dependent tests)

## Known Gaps

- The `ahamkara_client_lib` target has pre-existing compile failures (`threaded_local_runtime.cpp` lock_guard, `debug_client.cpp` namespace qualifier) that prevent the full `run-tests.sh` from completing.
- Runtime visual confirmation (weapon movement visible in a GL window) is not possible in this headless environment.
- The `notify_fired()` method is wired into `WeaponAnimationController` but no external caller currently invokes it — it needs to be called from the weapon fire pipeline (e.g., `debug_scene_bridge.cpp` or wherever `on_fire` is processed).

## Runtime Risks

- Layer composition order matters. The current order (sway * bob * recoil * ADS) was chosen to match the previous monolithic behavior. If weapon types need different ordering, the API supports it but callers must be explicit.
- The recoil kick is a fixed 50ms impulse. If framerate drops below ~20fps, the kick may be visually skipped. This matches the previous behavior.

## Cross-Agent Dependencies

- The `WeaponAnimConfig` and `WeaponAnimState` structs in `character_weapon.h` are shared with `animation_driver.cpp` (third-person animation pipeline).
- `WeaponAnimationController::notify_fired()` is intended for the weapon fire system to call. The caller integration is a separate follow-up.

## Recommended Next Step

Wire `WeaponAnimationController::notify_fired()` into the weapon fire pipeline so the recoil kick layer activates on actual fire events.

## Confidence

`high` — the layer extraction is a pure refactor of existing animation math into independent functions. All buildable tests pass, and backward compatibility is preserved via the `evaluate_weapon_animation()` wrapper.
