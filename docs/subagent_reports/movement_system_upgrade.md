# Task

Implement a competitive FPS movement upgrade across all 10 focus areas (31–40) in the Ahamkara C++20 engine, spanning acceleration, slope/step, collision feedback, jump buffering, slide/sprint refinement, ladder/ledge, moving platforms, head bob, debug visualization, and surface materials.

# Outcome

All 10 areas are at least partially implemented. The core movement math (areas 31, 34) is fully implemented and test-covered. The Jolt integration (areas 32, 33, 37, 40) is wired through the contact listener and velocity pipeline. Ladder/ledge/mantle (area 36) has ladder detection and vertical climb working; ledge grab and full mantle animation are stubs with config constants ready. Head bob and debug state (areas 38, 39) are fully populated per tick.

| Area | Status |
|------|--------|
| 31 — Acceleration model | ✅ Full Quake/Source implementation |
| 32 — Ground/air/slope/step | ✅ Slope detection + slide-down physics; step height constant exposed |
| 33 — Capsule collision feedback | ✅ Ground normal + surface material fed from Jolt to movement system |
| 34 — Jump buffering + coyote time | ✅ Full implementation with timer decay and did_jump flag |
| 35 — Crouch/slide/sprint/mantle | ✅ Slide cooldown, sprint turn penalty, sprint accel multiplier |
| 36 — Ladder and ledge movement | ✅ Ladder climb + dismount; ledge state stubbed |
| 37 — Moving platform support | ✅ Delta-tracking via ground body ID |
| 38 — Head bob + camera impulse | ✅ Sinusoidal bob + landing impulse, applied per tick |
| 39 — Movement debug visualization | ✅ `MovementDebugState` populated, exposed via `get_movement_debug()` |
| 40 — Surface material modifiers | ✅ 11-material enum with per-material speed/friction tables; tagged on colliders |

# Files Changed

- `game/include/ahamkara/game/net_types.h` — Added `MovementState` values: `OnLadder`, `LedgeGrab`, `Mantling`
- `game/include/ahamkara/game/movement.h` — Added `SurfaceMaterial` enum (11 values), expanded `MovementConfig` (37 fields with defaults), expanded `MovementSimState` (20 fields), new `MovementDebugState` struct, 7 new public API functions
- `game/src/movement.cpp` — Implemented `compute_slope_angle`, `apply_slope_physics`, `compute_head_bob_offset`, `compute_landing_impulse`, `surface_speed_multiplier`, `surface_friction_multiplier`; extended `accelerate_movement` with surface multipliers, slide cooldown, sprint turn penalty, and new movement states
- `game/include/ahamkara/game/debug_map.h` — Added `SurfaceMaterial surface_material` field to `ColliderBox`
- `game/include/ahamkara/game/world.h` — Added `MovementDebugState movement_debug_` member; new methods `resolve_ladder_and_ledge`, `resolve_moving_platform`, `populate_movement_debug`; changed `update_camera` signature to take `float delta_seconds`; added `get_movement_debug()` accessor; added `#include <cstdint>`
- `game/src/world.cpp` — Contact listener captures ground normal + surface material; velocity block uses surface-aware multipliers; added slope physics after acceleration; added `resolve_moving_platform`, `resolve_ladder_and_ledge`, `populate_movement_debug`; `update_camera` now applies head bob + landing impulse; call sites updated in `tick_internal`
- `tests/src/movement_tests.cpp` — 14 tests: acceleration ramp-up, convergence, friction, air control, jump buffer, buffer expiry, coyote time, coyote expiry, gravity, ground clamp, movement state resolution, backward compat, custom config, ground detection
- `tests/src/local_play_tests.cpp` — Updated position assertion (tolerance for acceleration ramp-up) and camera assertion (tolerance for head bob offset)
- `tests/src/world_tests.cpp` — Updated camera assertion (tolerance for head bob offset)
- `tests/CMakeLists.txt` — Added `ahamkara_movement_tests` target with `ae_core` + `ahamkara_game` dependencies
- `game/include/ahamkara/game/client_prediction.h` — Fixed `usize` → `ae::usize` (pre-existing bug)

# Interfaces Added Or Changed

**New enums:**
- `SurfaceMaterial` (`std::uint8_t`): `Default`, `Concrete`, `Metal`, `Wood`, `Dirt`, `Grass`, `Ice`, `Mud`, `Sand`, `Ladder`, `Count`

**New `MovementState` values** (appended to existing enum; serialized as `ae::u8`):
- `OnLadder`, `LedgeGrab`, `Mantling`

**New structs:**
- `MovementDebugState` (8 fields: velocity, wish direction, ground normal, jump buffer %, coyote %, slide %, on-ground, on-ladder, surface material)

**Changed structs:**
- `MovementConfig` — grew from 14 to 37 fields (added speeds, slope/step, slide, sprint, ladder/ledge, head bob, per-surface arrays)
- `MovementSimState` — grew from 3 to 20 fields (added slide cooldown, ladder/ledge/mantle state, moving platform tracking, head bob phase, ground contact info)
- `ColliderBox` — added `SurfaceMaterial surface_material` field with `Default` initializer

**New public API functions (in `ahamkara::game` namespace):**
- `float compute_slope_angle(const Vec3& ground_normal)`
- `void apply_slope_physics(Vec3& velocity, const Vec3& ground_normal, float delta_seconds, const MovementConfig& config)`
- `Vec3 compute_head_bob_offset(float current_speed, float max_speed, float& phase, float delta_seconds, const MovementConfig& config)`
- `Vec3 compute_landing_impulse(float impact_speed, const MovementConfig& config)`
- `float surface_speed_multiplier(SurfaceMaterial mat, const MovementConfig& config)`
- `float surface_friction_multiplier(SurfaceMaterial mat, const MovementConfig& config)`

**Changed function signatures:**
- `World::update_camera(const PlayerInputCommand& input)` → `World::update_camera(const PlayerInputCommand& input, float delta_seconds)`

**New World methods:**
- `const MovementDebugState& get_movement_debug() const`
- `void resolve_ladder_and_ledge(const PlayerInputCommand& input)` — private
- `void resolve_moving_platform(float delta_seconds)` — private
- `void populate_movement_debug(float delta_seconds, const PlayerInputCommand& input)` — private

**New test target:**
- `ahamkara_movement_tests` — depends on `ae_core` + `ahamkara_game`, 14 test cases

# Behavior

**Acceleration (area 31):** Players no longer snap to max speed instantly. Velocity ramps up over ~100ms at 60fps from ground_accel. Friction decelerates when input is released. Air control is severely limited (air_accel = 1.5 vs ground_accel = 12.0).

**Jump buffering + coyote time (area 34):** Pressing jump up to 0.15s before landing triggers jump on contact. Running off a ledge gives a 0.10s grace window to still jump. Both are configurable via `MovementConfig`.

**Slope physics (area 32):** Surfaces steeper than 45° cause downhill slide acceleration. The slope angle is computed from the Jolt ground normal. Step height is exposed as a config constant.

**Collision feedback (area 33):** The Jolt contact listener now writes ground normal and surface material into `MovementSimState` on every validated contact.

**Slide/sprint (area 35):** Slide has a cooldown (0.8s). Sprinting has a sharp-turn speed penalty (0.85x multiplier when wish direction differs >45° from velocity).

**Ladders (area 36):** Collider bodies tagged `SurfaceMaterial::Ladder` trigger ladder state. Vertical input controls climb speed (4.0 m/s). Dismounts on reaching ground. Ledge grab and full mantle are stubs.

**Moving platforms (area 37):** When the character stands on a non-static Jolt body, the position delta is tracked and applied each tick, enabling platform riding.

**Head bob (area 38):** Camera receives a sinusoidal vertical+horizontal offset scaled by speed. Landing produces a one-shot downward impulse proportional to impact speed.

**Debug visualization (area 39):** `MovementDebugState` is populated each tick with velocity, wish direction, ground normal, timer percentages, and on-ground/on-ladder flags. Exposed via `World::get_movement_debug()`.

**Surface materials (area 40):** 11 materials with per-material speed and friction multiplier arrays. Ice is nearly frictionless (0.05x). Mud is sticky (2.0x friction). Speed penalties on sand (0.7x) and mud (0.5x). Default material on all existing colliders preserves original behavior.

# Validation

**Build:**
```
cmake --build build --target ahamkara_game          → passed
cmake --build build --target ahamkara_movement_tests → passed
cmake --build build --target ahamkara_world_tests    → passed
cmake --build build --target ahamkara_smoke_tests    → passed
```

**Tests:**
```
ctest --test-dir build → 6/6 passed (100%)
  ahamkara_smoke_tests        → passed (0.03s)
  ahamkara_world_tests        → passed (0.03s)
  ahamkara_movement_tests     → passed (14 tests, 0.00s)
  ahamkara_collision_tests    → passed
  ahamkara_gameplay_tests     → passed
  ahamkara_asset_pipeline_tests → passed
```

**Warnings:** Two deprecation warnings from EnTT's `hashed_string.hpp` (pre-existing; `operator"" _hs` whitespace deprecation in C++20).

# Known Gaps

- **Ledge grab (area 36):** `resolve_ladder_and_ledge` has a stub comment for ledge detection. Config constants (`ledge_grab_range`, `mantle_up_speed`) are declared but the ledge raycast and pull-up sequence are not implemented.
- **Mantle animations (area 36):** The existing `resolve_mantle()` function is untouched. Full vault-vs-climb distinction is not done.
- **Ladder dismount direction:** Currently snaps to ground below; no forward/backward dismount at top/bottom.
- **Head bob roll (area 38):** Roll angle is computed in `compute_head_bob_offset` and returned in the z component of the Vec3, but the caller (`update_camera`) only applies x/y; roll is not applied to camera transform.
- **Debug renderer (area 39):** `MovementDebugState` is populated but no debug draw calls consume it yet. The data is available for a debug renderer pass.
- **Moving platform edge cases (area 37):** Only tracks one moving body at a time. Does not handle rotation. Delta tracking uses `SetPosition` directly, bypassing Jolt collision on the platform step.
- **Per-surface config arrays:** Hardcoded in the `MovementConfig` struct. Not runtime-loadable from data files.
- **Sprint accel multiplier:** Set to `1.0F` (neutral) in both standalone and world paths. No stamina system exists.
- **`simulate_player_movement` backward compat:** Preserved exactly as it was — uses old instant-velocity model, no surface/slope/ladder awareness.

# Risks

- **Network serialization:** `MovementState` grew from 5 to 8 values. Existing serialization uses `static_cast<ae::u8>()`, so new values will serialize correctly. However, old clients won't understand `OnLadder`/`LedgeGrab`/`Mantling` states from a new server — they'd see an unknown byte value.
- **ColliderBox ABI:** Added `SurfaceMaterial` field. Existing `kDebugMapColliders` (42 entries) all use the `Default` initializer so behavior is unchanged. External code constructing `ColliderBox` structs without the new field will get zero-initialized `SurfaceMaterial::Default`.
- **Camera test tolerance:** Camera Y assertions now use `< 0.1F` tolerance instead of exact equality. This is safe (max head bob is 0.025m) but means tests won't catch accidental 5cm camera offsets.
- **Jolt contact listener mutability:** The contact listener now writes to `movement_sim_state_`, which required changing `const World*` to `World*`. This is thread-safe in the current single-threaded simulation, but would need synchronization if Jolt's collision detection ever runs on a job system thread.
- **`World::update_camera` signature change:** Any external code calling `update_camera` directly (unlikely since it's private) will fail to compile.

# Next Recommended Steps

1. **Implement ledge grab raycast** in `resolve_ladder_and_ledge`: cast forward from player eye position, detect ledges within `ledge_grab_range`, transition to `LedgeGrab` state, then mantle up.
2. **Wire debug renderer** to consume `MovementDebugState`: draw velocity vector line, wish-direction arrow, ground normal, and HUD-style timer bars for jump buffer / coyote / slide.
3. **Add surface material tagging to the debug map**: assign `Ice`, `Mud`, `Sand` materials to specific collider entries in `kDebugMapColliders` to exercise area 40 gameplay.
4. **Add movement tests for new functions**: `compute_slope_angle`, `compute_head_bob_offset`, `surface_speed_multiplier` with non-default materials.
5. **Apply head bob roll** to the camera transform in `update_camera` (the z component of the offset Vec3 is currently ignored).
6. **Add moving platform rotation support**: track body quaternion delta and apply to character.
7. **Implement sprint stamina**: add a stamina float to `ReplicatedPlayerState` or `MovementSimState`, drain while sprinting, regenerate while idle/walking.

# Notes For Integration

- The `MovementConfig` struct is large (37 floats + 2 arrays). If you need to network this, consider a compressed representation — most fields change rarely.
- All existing `ColliderBox` data (in `debug_map.h` and any runtime collider construction) will silently default to `SurfaceMaterial::Default` due to the zero-initialization of the new field.
- The `simulate_player_movement` function in `movement.cpp` is intentionally untouched. It serves as a reference for the old behavior and is still used by `network_smoke_tests.cpp` line 128.
- Two pre-existing bugs were fixed as part of this work: `usize` → `ae::usize` in `client_prediction.h` line 62/73 (build break), and the `update_camera(PlayerInputCommand{})` call in `set_player_state` needed updating to the new two-parameter signature.
