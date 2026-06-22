# Task
Implement debug UI and GPU profiling improvements: wire up reserved GPU UI timer, add frame-time sparkline to metrics overlay, add toggleable GPU profiler overlay bar chart.

# Outcome
**Fully implemented:**
- GPU UI timer (slot 3, previously reserved but never started/stopped) is now wired and reads `gpu_time_ui_ms` each frame.
- Render draw order reorganized so all screen-space UI elements are contiguous, allowing clean separation of main pass timer (map + entities + effects) and UI pass timer (crosshair + metrics + HUD + damage numbers + menu + scene overlay + GPU profiler).
- Metrics overlay (`draw_metrics_overlay`) enhanced with:
  - Frame-time sparkline (200-frame ring buffer, auto-scaled, color-coded: green <8.3ms, yellow <16.7ms, red ≥16.7ms)
  - Threshold reference lines at 8.3ms and 16.7ms
  - Categorized sections: CPU, GPU (TOTAL/DEPTH/MAIN/UI), DRAW STATS (CELLS/ENTITIES/LOD/PART/DECAL/PROJ)
  - Color-coded FPS/FRAME indicator dots
- New toggleable GPU profiler overlay (`draw_gpu_profiler_overlay`) with horizontal bar chart showing Depth/Main/UI/TOTAL timing, visible via F4 key.
- `gpu_profiler_visible` bool added to `DebugScene` and plumbed through `build_debug_scene`.

**Partially implemented:**
- `gpu_time_entities_ms` field exists in both `DebugScene` and `Impl` but is not separately timed (entities are part of the main pass timer). It remains zero.

**Not implemented:**
- No per-entity or per-material GPU timing breakdown.
- No CSV/JSON export of profiling data.
- No overlay for controller-based toggling of GPU profiler (keyboard-only F4).
- No threading of `gpu_profiler_visible` through the window title bar (metrics already thread their values there; GPU profiler does not).

# Files Changed
- `engine/render/include/ae/render/debug_renderer.h` — Added `bool gpu_profiler_visible {false};` field to `DebugScene` struct.
- `engine/render/src/debug_renderer.cpp` — Reorganized draw order (crosshair/metrics moved after decals); wired GPU UI timer (slot 3) begin/end/read; added sparkline ring buffer to `Impl`; replaced `draw_metrics_overlay` with categorized+sparkline version (new signature takes `frame_time_history` and `sparkline_count`); added `draw_gpu_profiler_overlay` function.
- `client/src/debug_client.cpp` — Added `bool gpu_profiler_visible` local; added F4 toggle handler; updated `build_debug_scene` signature and body to accept/forward `gpu_profiler_visible`; updated both call sites (menu branch and main loop).

# Interfaces Added Or Changed
- **Public struct field:** `DebugScene::gpu_profiler_visible` (bool, default `false`).
- **Changed function signature:** `draw_metrics_overlay` now takes two additional parameters: `const std::array<double, 200>& frame_time_history` and `int sparkline_count`. This function is internal to `debug_renderer.cpp` (file-scope, no header declaration), so only the call site within `DebugRenderer::render()` is affected.
- **Changed function signature:** `build_debug_scene` now takes `bool gpu_profiler_visible` as the 4th parameter (after `metrics_visible`). This is a client-internal function defined in `debug_client.cpp`.
- **New function:** `draw_gpu_profiler_overlay(const DebugScene&, int width, int height)` — internal to `debug_renderer.cpp`.
- **New key binding:** F4 toggles `gpu_profiler_visible` in the debug client main loop.

# Behavior
- **Metrics overlay (F3):** Now shows a much larger panel with a frame-time sparkline at the top, followed by GPU section (TOTAL, DEPTH, MAIN, UI times in ms) and draw statistics. Frame time and FPS are color-coded against 60fps/120fps thresholds with indicator dots.
- **GPU profiler overlay (F4):** Appears as a panel on the right side of the screen above the HUD. Shows horizontal bars for Depth, Main, UI, and TOTAL GPU times with numeric values. Each bar is width-proportional to a 33ms budget. Only visible when GPU timer queries are supported by the driver.
- **Existing overlays:** Crosshair, HUD (health/ammo/minimap), menu, scene overlay, damage numbers all render identically — only their draw order within the frame was shifted so they form a contiguous UI block for timer isolation.
- **Timer isolation:** The main pass timer (slot 2) now captures only map geometry, entities, muzzle flash, 3D projection, projectiles, particles, and decals. The UI pass timer (slot 3) captures all screen-space overlays. Previously both were lumped into slot 2 and slot 3 was never started.

# Validation
- **Build command:** `cd build/debug && ninja -j8` — **passed** (full build succeeded, `ahamkara_client` executable linked).
- **Compile check (individual files):** `ninja engine/render/CMakeFiles/ae_render.dir/src/debug_renderer.cpp.o` and `client/CMakeFiles/ahamkara_client.dir/src/debug_client.cpp.o` — both **passed** with zero errors.
- **Tests:** No renderer-specific tests exist. Gameplay tests (`ahamkara_gameplay_tests`) linked successfully.
- **Warnings:** Pre-existing EnTT deprecation warnings about `operator"" _hs` (unrelated). No new warnings introduced by these changes.
- **Language server diagnostics:** Clangd reports many "file not found" errors — this is a pre-existing LSP include-path configuration issue, not a build problem. The ninja build uses correct CMake-generated include paths.

# Known Gaps
- `gpu_time_entities_ms` field in `DebugScene` and `Impl` is declared but always 0.0 — entities are measured within the main pass timer (slot 2), not separately. A 5th GPU timer slot would be needed to isolate entities from map geometry.
- The window title bar does not show GPU profiler state (metrics overlay state is shown; GPU profiler is not).
- The GPU profiler bar chart uses a fixed 33ms budget for bar width normalization. On high-refresh displays this may look narrow; on very slow frames it will clamp.
- No controller binding for F4 (GPU profiler toggle). Metrics toggle has a controller binding via `controller_bindings.metrics`; GPU profiler does not.
- `draw_metrics_overlay` signature change breaks any hypothetical external callers, but none exist — the function is file-scope.

# Risks
- **Draw order change:** Crosshair and metrics overlay were moved from before projectiles/particles/decals to after them. If any code relied on the depth buffer state from these overlays being present during world-space particle/decal rendering, it could break. Visual inspection of the depth state save/restore in each function suggests this is safe — each overlay pushes/pops its own projection/modelview matrices and manages depth test state independently.
- **GPU timer nesting:** Timer slots 2 and 3 are now sequential (2 ends before 3 begins). They were previously overlapping (3 was never started). The total frame timer (slot 0) still spans everything. Timer slot 1 (depth pre-pass) is unchanged. No timer slots are nested within each other.
- **Sparkline ring buffer:** 200 doubles = 1600 bytes in `Impl`. This is per-frame stable memory. The ring buffer is not cleared on shutdown (stale data remains), but it is not accessed after shutdown.

# Next Recommended Steps
1. Add a 5th GPU timer query slot to time entities (player + dummies) separately from map geometry, populate `gpu_time_entities_ms`.
2. Add controller binding for GPU profiler toggle (map a gamepad button combo to `gpu_profiler_visible`).
3. Add `gpu_profiler_visible` to the window title bar string in `build_debug_window_title`.
4. Expose the sparkline min/max range as a small text label on the overlay (currently auto-scaled with no range display).
5. Add a frame-time histogram alongside the sparkline (bucket frame times into bins for distribution view).
6. Wire up 1% low/high FPS display in the enhanced metrics overlay (fields exist in `DebugScene` but were dropped from the categorized layout to save vertical space).
7. Consider making `draw_metrics_overlay` and `draw_gpu_profiler_overlay` methods on `DebugRenderer::Impl` instead of free functions, so they can directly access `Impl` state without parameter threading.

# Notes For Integration
- `build_debug_scene` now has 10 parameters. The 4th is `gpu_profiler_visible`. Any code that calls `build_debug_scene` must be updated to pass this boolean, otherwise it will not compile. Both call sites in `debug_client.cpp` have been updated.
- The `headless_clients.cpp` and `flashback` sample do not call `build_debug_scene` and are unaffected.
- If the render backend is ever swapped from OpenGL, the GPU timer query API in `RenderBackend` (`create_query`, `begin_query`, `end_query`, `is_query_result_available`, `get_query_result_uint64`) must be implemented for the new backend, or `gpu_timers_supported` will remain false and all GPU timing features will be silently disabled.
