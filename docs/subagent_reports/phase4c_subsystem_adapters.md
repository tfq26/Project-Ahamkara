# Phase 4C: Subsystem Adapter Layer — Report

## Date

2026-06-06

## Summary

Created three narrow subsystem adapters that let dormant systems (animation, audio
event routing, movement debug visualisation) attach to the game runtime through
clean, explicit boundaries. No subsystem internals were modified, and no new link
dependencies were introduced into headless/server targets.

## Files Created

| File | Purpose |
|------|---------|
| `game/include/ahamkara/game/adapters/animation_adapter.h` | Converts game types → `AnimGameplayInput`, wraps `AnimationDriver` with state machines |
| `game/include/ahamkara/game/adapters/audio_event_adapter.h` | Factory helpers for common `AudioEvent` construction + surface-type mapping |
| `game/include/ahamkara/game/adapters/movement_debug_adapter.h` | Snapshot extraction and debug-line generation for movement visualisation |

## Files Modified

None — all adapters are new, header-only additions. No existing code was touched.

## Adapter Details

### 1. Animation Adapter (`animation_adapter.h`)

**Namespace:** `ahamkara::game::adapters`

**What it provides:**

- `game_to_anim_movement_state(MovementState)` — exhaustive mapping from all
  `game::MovementState` variants to `ae::animation::AnimMovementState`.
- `build_anim_gameplay_input(player_state, input, sim_state)` — constructs a
  complete `AnimGameplayInput` from game-side state in one call.
- `AnimAdapter` class — wraps an `AnimationDriver`, two `StateMachine`s
  (locomotion + upper body), an `AnimationGraph`, and a `WeaponAnimState`.
  Provides `init()` for stock FPS state machine setup and `tick_from_game()`
  for single-call update.

**Boundary rule:**

- This header includes `ae/animation/animation_driver.h`, which transitively
  depends on `ae/render`. It must only be included in targets that link
  `ae_animation` (i.e., client builds). The headless server never includes it.
- The `ahamkara_game` library does **not** link `ae_animation`, so this
  adapter is compiled only when a client-side translation unit includes it.

**Integration point (future):**

```cpp
// In client game loop (links both ahamkara_game + ae_animation):
adapters::AnimAdapter anim;
anim.init();
// ...
anim.tick_from_game(world.get_player_state(), sim_state, input, dt, out_pose, out_weapon);
renderer.submit_skinned_mesh(character_model, out_pose);
```

### 2. Audio Event Adapter (`audio_event_adapter.h`)

**Namespace:** `ahamkara::game::adapters`

**What it provides:**

- `surface_material_to_audio_surface(SurfaceMaterial)` — maps the 10
  `game::SurfaceMaterial` values to `AudioEvent::SurfaceType` values.
- Factory functions with sensible defaults for volume, category routing, and
  2D/3D flag:
  | Function | Default Category | Default Volume |
  |---|---|---|
  | `make_weapon_fire_event()` | `AudioCategory::Weapon` | 1.0 |
  | `make_weapon_reload_event()` | `AudioCategory::Weapon` | 0.7 |
  | `make_jump_event()` | `AudioCategory::SFX` | 0.5 |
  | `make_landing_event()` | `AudioCategory::SFX` | scales with impact speed |
  | `make_footstep_event()` | `AudioCategory::SFX` | 0.4 |
  | `make_hit_event()` | `AudioCategory::SFX` | 0.8 |
  | `make_bullet_impact_event()` | `AudioCategory::SFX` | 0.6 |
  | `make_ui_event()` | `AudioCategory::UI` | 0.6, `is_2d = true` |

**Boundary rule:**

- All functions only populate `AudioEvent` structs. They never call
  `IAudioPlayer`. Headless/server code can call these safely — events are
  discarded when no audio player is set.
- No new includes beyond what `ahamkara_game` already uses.

**Integration point (future):**

```cpp
// In World::tick_internal, instead of inline AudioEvent construction:
world.queue_audio_event(adapters::make_jump_event(player_state_.position));
world.queue_audio_event(adapters::make_landing_event(
    player_state_.position, impact_speed, sim_state.ground_material));
```

### 3. Movement Debug Adapter (`movement_debug_adapter.h`)

**Namespace:** `ahamkara::game::adapters`

**What it provides:**

- `MovementDebugSnapshot` — a flat, trivially-copyable struct containing only
  the debug-relevant fields from `MovementDebugState`. Decouples the debug
  renderer from simulation internals.
- `extract_movement_debug_snapshot(MovementDebugState)` — extracts a
  renderer-consumable snapshot in one call.
- `DebugLine` and `MovementDebugLines` — simple line-buffer types for debug
  drawing.
- `make_movement_debug_lines(snapshot, origin, out_lines)` — generates
  coloured debug lines for velocity (cyan), wish direction (yellow), ground
  normal (green), jump buffer (white), coyote time (magenta cross), slide
  timer (orange), and ladder state (blue).
- `surface_material_debug_color(SurfaceMaterial, &r, &g, &b)` — maps surface
  materials to debug tint colours.

**Boundary rule:**

- No render or animation dependencies. The snapshot is a plain data struct;
  the debug line generation produces abstract line data that any renderer can
  consume. Safe for headless/server — building a snapshot is cheap and
  discarded when no debug renderer is attached.

**Integration point (future):**

```cpp
// In debug renderer, after world tick:
auto snap = adapters::extract_movement_debug_snapshot(world.get_movement_debug());
adapters::MovementDebugLines lines;
adapters::make_movement_debug_lines(snap, world.get_player_state().position, lines);
for (int i = 0; i < lines.count; ++i) {
    debug_renderer.draw_line(lines.lines[i]);
}
```

## Headless/Server Safety Verification

| Adapter | Depends on render/audio? | Safe for `ahamkara_server`? |
|---------|--------------------------|---------------------------|
| `animation_adapter.h` | Yes (includes `ae/animation` → `ae/render`) | N/A — excluded by include-path guard; never compiled into server targets |
| `audio_event_adapter.h` | No | Yes — struct construction only, no playback calls |
| `movement_debug_adapter.h` | No | Yes — plain data extraction, no rendering |

## Build and Test Verification

### Compilation validation

All three adapter headers were validated with `clang++ -std=c++20 -fsyntax-only`:

- `audio_event_adapter.h` — compiles with game-layer include paths only ✅
- `movement_debug_adapter.h` — compiles with game-layer include paths only ✅
- `animation_adapter.h` — compiles with game + animation + render include paths ✅
- All three headers coexist with existing game headers (`movement.h`,
  `audio_events.h`, `net_types.h`) without name collisions or ambiguous
  overloads ✅

### Existing test suite

| Test suite | Status | Notes |
|------------|--------|-------|
| `ahamkara_collision_tests` (8 test groups) | ✅ All passed | Not affected by adapters |
| `ahamkara_movement_tests` (14 tests) | ✅ All passed | Compiled and run standalone; not affected by adapters |
| `ahamkara_world_tests` | ⚠️ Pre-existing build error | `world.cpp` has unresolved `GamePhysics`/`Layers` symbols (unrelated to Phase 4C) |

### What was NOT changed

- `game/src/world.cpp` — untouched; no adapter calls inserted
- `game/CMakeLists.txt` — no new link dependencies added
- `engine/animation/*` — no engine code modified
- Any existing header or source file — zero modifications

## Design Decisions

1. **Header-only adapters.** All three adapters are inline-function headers.
   This avoids adding new translation units to the build graph and makes them
   trivial to include or exclude from any target.

2. **Animation adapter is client-gated.** Rather than creating a link
   dependency from `ahamkara_game` → `ae_animation` (which would pull render
   into the server), the adapter header is only compilable in targets that
   already have `ae_animation` in their include path. The doc comment
   explicitly states this constraint.

3. **Audio adapter produces events, never plays them.** All factory functions
   return `AudioEvent` structs. The caller is responsible for queuing and
   flushing. This preserves the existing `World::queue_audio_event` →
   `World::flush_audio_events` → `IAudioPlayer::play_event` dispatch chain.

4. **Movement debug snapshot is a copy.** The `MovementDebugSnapshot` is
   extracted by value from `MovementDebugState`, so the debug renderer can
   hold a snapshot without worrying about the simulation mutating state
   mid-frame.

5. **No broad subsystem rewrite.** These adapters are thin conversion layers
   (~170, ~180, ~230 lines each). They add surface area for integration
   without changing how the underlying subsystems work.

## Next Steps

1. **Wire the audio event adapter** into `World::tick_internal` — replace
   any inline `AudioEvent{}` construction with adapter factory calls.
2. **Wire the animation adapter** into the client game loop — construct an
   `AnimAdapter`, call `tick_from_game()` each frame, feed output joint
   matrices to the renderer.
3. **Wire the movement debug adapter** into the debug renderer — replace
   direct `MovementDebugState` reads with snapshot extraction and line
   generation.
4. **Fix pre-existing `world.cpp` build errors** (`GamePhysics`/`Layers`
   forward-declaration issues) so the full test suite can run.
