# Phase 2E: Decomposition Cleanup and Verification

**Date:** 2026-06-06
**Status:** Complete

## Summary

Focused cleanup of issues introduced or exposed by the Phase 2A–2D engine subsystem decomposition passes. The goal was to make the decomposed layout feel intentional, reduce include/layout friction, and tighten build/test confidence.

## Issues Found and Fixed

### 1. Missing include in `debug_renderer_internal.h`

**File:** `engine/render/src/debug_renderer_internal.h`

The internal header used `ae::render::Mat4` (at line 109) but did not include `ae/render/skeletal_animation.h` where the type is defined. This caused compilation errors in `debug_renderer_gpu.cpp` and any other `.cpp` file that included the internal header.

**Fix:** Added `#include "ae/render/skeletal_animation.h"` to the header's include list.

### 2. Private nested type accessibility in `debug_renderer.h`

**File:** `engine/render/include/ae/render/debug_renderer.h`

The `DebugRenderer::Impl` forward declaration was in the `private:` section. Free functions in `debug_renderer.cpp` (later moved to be `Impl` member functions) referenced `DebugRenderer::Impl&` in their signatures, causing access-control errors.

**Fix:** Moved `struct Impl;` to the public section of `DebugRenderer`, leaving only `std::unique_ptr<Impl> impl_` as private. This is safe — the PIMPL idiom is preserved since `Impl`'s full definition remains in the `.cpp` file.

### 3. Premature namespace closure in `debug_renderer.cpp`

**File:** `engine/render/src/debug_renderer.cpp`

A stray `}  // namespace` at line 1430 closed `namespace ae::render` prematurely, causing `DebugRenderer::render()` and all subsequent code to be compiled outside the namespace. This resulted in "undeclared identifier `DebugRenderer`" errors.

**Fix:** Removed the spurious namespace closing brace. The correct closing brace at the end of the file (line 1725) was retained.

### 4. Call sites not updated after method extraction

**File:** `engine/render/src/debug_renderer.cpp`

During decomposition, `draw_sky_pass`, `draw_depth_pre_pass`, and `draw_main_color_pass` were converted from free functions (with `DebugRenderer::Impl&` as first parameter) to member functions of `DebugRenderer::Impl`. However, the call sites inside `DebugRenderer::render()` still used the old calling convention:
- `draw_sky_pass(scene, ...)` → `impl_->draw_sky_pass(scene, ...)`
- `draw_depth_pre_pass(*impl_, scene, frustum)` → `impl_->draw_depth_pre_pass(scene, frustum)`
- `draw_main_color_pass(*impl_, scene, ...)` → `impl_->draw_main_color_pass(scene, ...)`

**Fix:** Updated all three call sites to use the member function calling convention.

### 5. Redundant linker dependencies causing "ignoring duplicate libraries" warnings

Multiple test executables and the client/server binaries explicitly linked `ae_core` and `ae_platform` even though these came transitively through their other dependencies (e.g., `ae_render` PUBLIC-links `ae_core` and `ae_platform`, `ahamkara_game` PUBLIC-links `ae_core`).

**Files changed:**
- `tests/CMakeLists.txt` — removed redundant `ae_core` from 5 test targets
- `client/CMakeLists.txt` — removed redundant `ae_core` and `ae_platform`
- `server/CMakeLists.txt` — removed redundant `ae_core`
- `tools/CMakeLists.txt` — removed redundant `ae_core` and `ae_platform` from `ahamkara_controller_mapper`
- `samples/flashback/CMakeLists.txt` — removed redundant `ae_core` and `ae_platform`

### 6. Unused `[[nodiscard]]` return values

**Files:**
- `client/src/headless_clients.cpp:399` — `interpolator.get_bracketing_snapshots()` return value was ignored
- `tests/src/collision_tests.cpp` — 9 instances of `world.add_body()` return value ignored

**Fix:** Added `(void)` casts to explicitly discard the return values.

## Build/Test Results

| Metric | Before | After |
|--------|--------|-------|
| Build status | FAIL (compilation errors) | **PASS** (245/245) |
| Test status | 6/6 pass | 6/6 pass |
| Project warnings | 12 (errors + warnings) | **0** |
| Third-party warnings | 2 (enTT deprecation) | 2 (enTT deprecation, unchanged) |
| Linker duplicate warnings | 6 | **0** |

## Files Modified

| File | Change |
|------|--------|
| `engine/render/src/debug_renderer_internal.h` | Added missing `skeletal_animation.h` include |
| `engine/render/include/ae/render/debug_renderer.h` | Moved `Impl` forward decl from private to public |
| `engine/render/src/debug_renderer.cpp` | Removed premature namespace closure; updated 3 call sites |
| `tests/CMakeLists.txt` | Removed 5 redundant `ae_core` links |
| `client/CMakeLists.txt` | Removed redundant `ae_core` and `ae_platform` links |
| `server/CMakeLists.txt` | Removed redundant `ae_core` link |
| `tools/CMakeLists.txt` | Removed redundant `ae_core` and `ae_platform` links |
| `samples/flashback/CMakeLists.txt` | Removed redundant `ae_core` and `ae_platform` links |
| `client/src/headless_clients.cpp` | `(void)` cast for unused nodiscard return |
| `tests/src/collision_tests.cpp` | 9 `(void)` casts for unused nodiscard returns |

## Observations

The decomposition structure is solid:
- 7 engine subsystems (`animation`, `collision`, `core`, `network`, `platform`, `render`, `runtime`) each with clean `include/` and `src/` separation
- Game logic in `game/` with its own namespace path `ahamkara/game/`
- Client/server/tools as consumers of the engine libraries
- All include paths use namespace-prefixed directories (e.g., `ae/core/types.h`, `ahamkara/game/world.h`)

The `game/src/` directory contains helper files (`world_camera.*`, `world_dummy_sim.*`, `world_jolt_bridge.*`, `world_projectile.*`) that are correctly listed in `game/CMakeLists.txt`. These are implementation details of the World class extracted to keep `world.cpp` manageable — not a decomposition issue.
