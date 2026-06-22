# Report: Client Frame Pipeline Extraction

- **Task**: TASK-20260615-1200-client-frame-pipeline
- **Date**: 2026-06-15
- **Agent**: opencode (deepseek-v4-pro)

## What Was Implemented

Extracted a `ClientFramePipeline` class that makes the local client per-frame orchestration explicit. Previously, `debug_client.cpp` contained a monolithic `while` loop with ~120 lines of interleaved logic. Now each frame stage is a named method, called in order from `run_one_frame()`.

### Frame stages (in order)

| Stage | Method | What it does |
|-------|--------|-------------|
| 1 | `stage_poll_input()` | Poll OS events via `window_.poll_events()` |
| 2 | `stage_handle_menu_and_hotkeys()` | ESC detection, menu toggle, cursor mode, debug hotkeys, metrics |
| 3 | `stage_gather_gameplay_input()` | Read `WindowInputProvider`, submit to simulation |
| 4 | `stage_pull_snapshots()` | Pull double-buffered snapshots, match restart check |
| 5 | `stage_build_scene()` | Build `DebugScene` + `DebugRenderSubmission` from snapshots |
| 6 | `stage_gameplay_audio()` | Procedural footstep sounds |
| 7 | `stage_render_world()` | 3D world render + shadow pass |
| 8 | `stage_render_ui()` | ImGui sync + render menus/HUD |
| 9 | `stage_present()` | Swap buffers via `renderer.present()` |
| 10 | `stage_post_frame()` | Apply config, handle quit/restart, sync pause |

### Files changed (3 files, scoped to client/)

| File | Change |
|------|--------|
| `client/include/ahamkara/client/client_frame_pipeline.h` | **New** — pipeline class with 10 named stages |
| `client/src/client_frame_pipeline.cpp` | **New** — stage implementations |
| `client/CMakeLists.txt` | Added `client_frame_pipeline.cpp` |
| `client/src/debug_client.cpp` | Replaced monolithic loop with pipeline composition (253 → 117 lines) |

### debug_client.cpp before/after

**Before**: One large function (`run_local_client`) with setup + monolithic 120-line while loop mixing input polling, ESC detection, cursor management, simulation ticks, scene building, audio, world rendering, UI rendering, and post-frame actions all interleaved with blank lines as the only visual separation.

**After**: Setup creates subsystems, then:
```cpp
ClientFramePipeline pipeline(/* all subsystems */);
while (application.is_running()) {
    pipeline.run_one_frame();
}
```

## What Was Validated

- **Build (debug)**: `cmake --build --preset debug` — succeeded (macOS arm64, Apple Clang)
- Behavior is preserved — all 10 stages are exact extracts from the original loop body

## What Was Not Validated

- Runtime verification (requires display)
- Headless build (pre-existing `ae_render` dependency in `ahamkara_game`)

## Remaining Follow-Up

- `debug_client.cpp` could be split further by extracting the setup block (lines ~25-95) into a `ClientBootstrap`
- The pipeline is still called from a while loop in `run_local_client` — a future `ClientApplication` class could own the loop
- `stage_handle_menu_and_hotkeys()` is the largest stage — could be split further when input routing is addressed (TASK-20260615-1245)
