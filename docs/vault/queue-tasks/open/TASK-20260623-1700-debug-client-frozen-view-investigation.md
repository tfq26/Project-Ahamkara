---
type: opencode-task
task_type: investigation
status: open
created: 2026-06-23
queued_by: opencode
assigned_to: codex
priority: high
escalation_tier: high
primary_reviewer: codex
secondary_reviewer:
requires_display: true
subsystems:
  - client
  - engine/render
related_feature:
report:
---

# TASK-20260623-1700-debug-client-frozen-view-investigation

## Goal

Investigate why the **local debug client's on-screen view appears frozen** —
looking (mouse) and moving (WASD) seem to have no visible effect — even though
the entire input -> camera -> render-data path has been instrumented and proven
correct. Two related input bugs were already found and fixed; this escalation is
for the remaining (suspected GL render/present) layer. Requires a GL display to
confirm.

## How To Reproduce

```sh
./scripts/start.sh local -- --level assets/compiled/levels/prototype_box.aelevel
```

Builds + launches the GUI client and loads a prototype level (ground grid + two
`test_box` mesh instances). Click Play to leave the menu, then try mouse-look and
WASD.

## Symptom

The displayed image does not visibly change with look or movement. Headless build
and tests are green; this is GL/display-only behavior.

## Already Fixed (uncommitted in working tree)

1. **Mouse-look was fully dead.** `GLFW_CURSOR_DISABLED` + `GLFW_RAW_MOUSE_MOTION`
   stops delivering trackpad deltas on macOS (the cursor-pos callback goes silent
   in disabled mode). FIX: disable raw mouse motion in
   `client/src/debug_client.cpp` (~line 44).
2. **Camera pitched straight up at the sky on unpause.** Input kept accumulating
   in `ThreadSafeInputProvider` while the sim was paused (sim thread wasn't
   draining it), then dumped ~3s of `look_delta` (e.g. `(663, 286)`) in a single
   tick -> `pitch` clamped to +89deg. FIX: drop input while paused in
   `client/src/threaded_local_runtime.cpp` `ThreadedLocalRuntime::submit_input`
   (early-return when `paused_`).

## Verified Correct (temporary `LookDiag` probes [A]-[E], still in tree)

Per-frame logs (category `LookDiag`) prove the full data path updates every frame:

- **[A]** `WindowInputProvider::gather_input` -> `look_delta` nonzero when moving.
- **[B]** `ThreadSafeInputProvider::gather_input` (sim thread) -> receives the
  accumulated `look_delta`.
- **[C]** `LocalPlaySimulation::tick` -> `steps_consumed=1` and
  `camera_anchor.yaw` genuinely changes (e.g. `0 -> -56 -> -164`); look is
  applied in `game/src/world_camera.cpp:21-22`.
- **[D]** `ClientFramePipeline::stage_pull_snapshots` -> `curr_snap_.camera_anchor.yaw`
  carries the new yaw on the render thread.
- **[E]** `ClientFramePipeline::stage_build_scene` -> the
  `scene_.camera_position/target` handed to the renderer are correct. After the
  pause fix, a representative in-game sample is:
  `pos=(-5.54, 0.58, -0.64) target=(-5.54, 0.58, 11.36)` — i.e. **level, facing
  +Z**, and `pos` tracks WASD movement (player left spawn `(-6,1.5,0)` and
  settled on the ground at eye height 0.58).

So client input, threaded sim, camera anchor, snapshot, scene-bridge camera, and
the `look_at(scene.camera_position, scene.camera_target)` inputs are all correct
each frame.

## Remaining Mystery (NOT yet investigated) — focus here

Despite `scene.camera_position/target` being correct every frame, the displayed
image does not appear to change. The unexamined layer is the GL render/present
path. Hypotheses to check:

- `debug_client.cpp` sets `renderer.set_auto_present(false)`. Confirm **who
  actually presents/swaps** the default framebuffer each frame, and that the
  world pass (not only ImGui) is presented.
- `engine/render/src/debug_renderer.cpp:1570` builds
  `view = look_at(scene.camera_position, scene.camera_target, up)`;
  `DebugRenderer::view_matrix()` (~760). Confirm the computed view + projection
  are actually **bound/used** by the world geometry and ground-grid shaders each
  frame (not a stale/identity/cached matrix).
- `client/src/debug_render_runtime.cpp:55-94` `render_local_debug_frame` ->
  `renderer.render(scene)` with a PBR world-phase hook. Confirm the world is
  drawn into the presented buffer.
- Rule out: window presenting only once; swapchain/double-buffer; an FBO that is
  never blitted; viewport/depth/cull state.

## First Read

- [Renderer backend](../../systems/renderer_backend.md)
- [Architecture](../../systems/architecture.md)
- This task's "Verified Correct" + "Remaining Mystery" sections.

## Likely Files

- `client/src/threaded_local_runtime.cpp` (threaded sim, snapshots, submit/gather, paused drop)
- `client/src/local_play.cpp` (`LocalPlaySimulation::tick`, interpolation accessors)
- `client/src/debug_scene_bridge.cpp` (`build_debug_scene`: camera at 202-204)
- `client/src/debug_render_runtime.cpp` (`render_local_debug_frame`)
- `engine/render/src/debug_renderer.cpp` (`render(scene)`, `look_at`:1570, `view_matrix`:760, present path)
- `client/src/debug_client.cpp` (window/renderer setup, `set_auto_present(false)`, level load)
- `game/src/world_camera.cpp` (`update_camera_state`)

## Working-Tree State

- The 2 fixes above are **uncommitted**.
- The `[A]-[E]` `LookDiag` probes and a level-load log are **uncommitted** and
  still present (useful for this investigation; OpenCode will strip them once
  resolved).

## Acceptance Bar

- A clear root cause for the frozen on-screen view (or confirmation that the two
  fixes already resolved it and only orientation/scene-sparsity remained), plus
  a concrete fix or precise findings.
- Confirm on a display that look + WASD visibly move the view and the two boxes
  can be brought into view (turn toward +X / yaw ~90).

## Validation

```sh
cmake --build --preset debug --target ahamkara_client
./scripts/start.sh local -- --level assets/compiled/levels/prototype_box.aelevel
```

Runtime/visual confirmation requires a GL display.

## Reporting Required

Standard: write a report in `docs/reports/subagents/`, append
`docs/reports/subagents/subagent-master-log.md`, update this task `report:` and
status, and move to `review-needed/` or `blocked/`.

## Notes For Codex

First confirm whether the camera is now level + facing +Z after the pause fix
(the latest `[E]` suggests it is). If the view is still frozen, the GL
present/draw layer (above) is the prime suspect, since every data-side probe is
already proven correct.
