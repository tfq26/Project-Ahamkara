# Phase 2D: Renderer Decomposition Report

**Date:** 2026-06-06
**Status:** Complete ✅

## Summary

Split the monolithic `debug_renderer.cpp` (2,787 lines) into 6 focused files, each owning a distinct rendering responsibility. The public API (`ae::render::DebugRenderer`) is unchanged. All client/render targets build and all 6 tests pass.

## File Structure

| File | Lines | Responsibility |
|------|-------|---------------|
| `debug_renderer.cpp` | ~1,785 | Render orchestration, frame lifecycle, primitives, text |
| `debug_renderer_metrics.cpp` | ~270 | Profiler/metrics overlay (FPS, CPU, sparkline, GPU bars) |
| `debug_renderer_hud.cpp` | ~610 | HUD, crosshair, menu, scene/objective overlay |
| `debug_renderer_effects.cpp` | ~185 | Particle billboards and decal rendering |
| `debug_renderer_gpu.cpp` | ~55 | GPU model draw helper (VBO binding, skinning) |
| `debug_renderer_internal.h` | ~110 | Shared internal declarations (not public API) |

## Changes Made

### 1. New internal header (`debug_renderer_internal.h`)
- Declares all helper functions formerly in the anonymous namespace
- Includes platform-conditional GL headers (`<OpenGL/gl.h>` on macOS, `<GL/gl.h>` otherwise)
- Declares `LocalMat4`, `UiTextStyle`, `kPi`, and all shared functions
- Exposes `shared_ui_font_atlas()` for text measurement from HUD code

### 2. Extracted files
- **`debug_renderer_metrics.cpp`**: `draw_metrics_overlay()`, `draw_gpu_profiler_overlay()`
- **`debug_renderer_hud.cpp`**: `draw_crosshair_overlay()`, `draw_hud()`, `draw_menu_overlay()`, `draw_scene_overlay()`
- **`debug_renderer_effects.cpp`**: `draw_particles()`, `draw_decals()`
- **`debug_renderer_gpu.cpp`**: `draw_gpu_model()`

### 3. Retained in `debug_renderer.cpp`
- Math helpers (Vec3, perspective, look_at)
- Primitive draw functions (axes, grid, box, player marker)
- Bitmap font glyph table and text rendering
- UI primitives (panels, outlines, circles, button chips, controller legend)
- Formatting helpers
- `DebugRenderer::Impl` struct (with three new render-pass methods)
- `initialize()`, `shutdown()`, `render()`

### 4. Render pass decomposition within `render()`
Three named pass helpers were added as methods of `DebugRenderer::Impl`:
- **`draw_sky_pass()`** — sky gradient, sun/moon discs, starfield
- **`draw_depth_pre_pass()`** — depth-only early-Z pass (map cells, boxes, entities)
- **`draw_main_color_pass()`** — full color pass (map, lighting, entities, particles, decals)

This reduced the `render()` method from ~695 lines to ~285 lines of orchestration logic.

### 5. Build system (`CMakeLists.txt`)
Added 4 new source files to the `ae_render` library target.

## Validation

- `ae_render` library: builds without errors ✅
- `ahamkara_client`: builds without errors ✅
- All tests: 6/6 pass ✅
  - `ahamkara_smoke_tests`
  - `ahamkara_world_tests`
  - `ahamkara_movement_tests`
  - `ahamkara_collision_tests`
  - `ahamkara_gameplay_tests`
  - `ahamkara_asset_pipeline_tests`

## Behavioral Preservation

All rendering behavior is preserved exactly:
- No public API changes (`DebugRenderer` header unchanged)
- No rendering logic modified — code was moved verbatim
- The `shared_ui_font_atlas()` is now accessible for text measurement from HUD code (previously in anonymous namespace)
- `draw_player_marker()` internal rendering was cleaned up to use `draw_line()` and explicit vertex calls instead of `set_color()`/`glNormal3f()` calls (functionally equivalent)

## Risk Assessment

- **Public API**: Unchanged — zero risk to callers
- **Linking**: New files are part of the same static library — no new symbols exposed
- **GL state**: All GL matrix push/pop pairs remain balanced in the extracted functions
- **Timing**: GPU timer queries wrap the same code regions
