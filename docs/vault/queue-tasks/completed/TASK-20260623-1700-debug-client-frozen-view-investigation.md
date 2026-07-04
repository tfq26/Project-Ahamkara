---
type: opencode-task
task_type: investigation
status: completed
created: 2026-06-23
queued_by: opencode
assigned_to: opencode
priority: high
escalation_tier: high
primary_reviewer: codex
secondary_reviewer:
requires_display: true
subsystems:
  - client
  - engine/render
related_feature:
report: ../../../reports/subagents/TASK-20260623-1700-debug-client-frozen-view-investigation-report.md
---

# TASK-20260623-1700-debug-client-frozen-view-investigation

## Goal

Investigate why the **local debug client's on-screen view appears frozen** —
looking (mouse/trackpad) and moving (WASD) seem to have no visible effect.
Several input-side bugs have now been found and fixed in the working tree.
Runtime confirmation still requires a GL display.

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

## Already Fixed / Changed (uncommitted in working tree)

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
3. **Idle/virtual gamepad suppressed keyboard and mouse.**
   `WindowInputProvider::gather_input` previously returned immediately whenever
   `window_.gamepad_state().connected` was true. That meant a connected idle
   controller could bypass WASD and mouse/trackpad input entirely. FIX:
   keyboard/mouse are now sampled as the baseline, and connected gamepad input
   layers on top.
4. **Platform input now reconciles polled GLFW state.**
   `engine/platform/src/window_glfw.cpp` still uses callbacks, but `poll_events`
   now reconciles important gameplay keys from `glfwGetKey` and reconciles
   cursor position from `glfwGetCursorPos` after `glfwPollEvents`. This covers
   macOS/trackpad disabled-cursor paths where GLFW's virtual cursor position may
   update even if cursor callbacks are sparse or silent.
5. **Prototype showcase spawn now faces the boxes.**
   `assets/levels/prototype_box.json` / `.lvl` now spawn at yaw `90`, looking
   from `(-6, 1.5, 0)` toward the two showcase boxes along +X. The compiled
   `assets/compiled/levels/prototype_box.aelevel` was regenerated with
   `ahamkara_asset_importer`.
6. **Level mesh placements now also produce debug-renderer boxes.**
   The loaded `LevelAsset` is retained by `debug_client.cpp`, passed into
   `ClientFramePipeline`, and used by `debug_scene_bridge.cpp` to add bright
   orange `DebugScene::level_boxes` at each mesh instance placement. This gives
   a fixed-function visibility fallback even if the PBR level-mesh path is hard
   to perceive or broken.
7. **Menu state is now separated from gameplay policy.**
   `ClientMenuState` owns an explicit `ClientMenuMode` (`Gameplay`, `MainMenu`,
   `PauseOverlay`, settings origins, `Character`) and exposes pause/cursor/input
   policy. `DebugUiController` now converts ImGui button outcomes into explicit
   transitions instead of letting raw `ae::ui::MenuState` mutations implicitly
   drive gameplay. `ClientFramePipeline` gates gameplay input and cursor capture
   through this owner.
8. **Core-profile PBR/shadow draws now bind VAOs.**
   On macOS core-profile OpenGL, `glVertexAttribPointer` requires a bound VAO.
   `PbrRenderer` and `ShadowPass` were setting vertex attribs without one, so
   level mesh/shadow draws could silently fail even though the level loaded.
   Both now own and bind VAOs around their attribute setup.
9. **Level-load snapshots are published immediately.**
   `LocalPlaySimulation::load_level` resets interpolation state, and
   `ThreadedLocalRuntime::load_level` publishes an initial snapshot immediately
   after loading. This prevents the menu/render path from starting with a
   zero-origin/default snapshot and then jumping to the level spawn after Play.
10. **Level debug boxes now write full depth.**
    `DebugRenderer::draw_depth_pre_pass` only wrote the +Z face for
    `DebugScene::level_boxes`, while the color pass uses `GL_EQUAL`. That made
    the orange fallback box visible from the menu angle but disappear after Play
    when the camera viewed side/front faces whose depth had not been written.
    FIX: level boxes now use `draw_box` in the depth pre-pass so all six faces
    participate in the equal-depth color pass.
11. **Map geometry was still using dead client-state calls.**
    The renderer already runs under a core-profile-style compat shim, where
    `glEnableClientState` / `glVertexPointer` are intentionally no-ops. The map
    cell draws in `debug_renderer.cpp` were still written as if those APIs were
    real fixed-function calls, so the geometry path could silently disappear.
    FIX: map cell triangles and lines now route through `gl_compat` draw
    helpers instead of direct client-state calls.

## Runtime Evidence

The old temporary `LookDiag` probes were removed. A short-lived `InputDiag` probe
was added and then removed after user runtime logs showed:

- menu state closes after Play (`menu_visible=0`)
- WASD samples are nonzero (`move=(1,0)`, `move=(0,-1)`, `move=(-1,0)`)
- simulation snapshots move (`pos=(-6,0,0)` to approximately
  `(-4.5,0,-1.1)`)
- firing reaches gameplay (`ammo` drops and `Fired: AR-15` logs)
- trackpad/mouse look samples are nonzero

## Remaining Mystery / Focus

After the input/menu/render fixes above, rerun on a display. The latest render
bug found explains the user report that the orange box was visible in the menu
but disappeared after Play. If the displayed image still does not change despite
the reoriented showcase spawn, PBR VAO fix, full debug-box depth pre-pass, and
the map-cell compat-shim migration, the next layer to inspect is GL
render/present. Hypotheses to check:

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
- `client/src/client_menu_state.cpp` / `.h` (gameplay-vs-menu mode ownership)
- `client/src/debug_ui_controller.cpp` (menu transition routing)
- `game/src/world_camera.cpp` (`update_camera_state`)

## Working-Tree State

- The fixes above are **uncommitted**.
- The old `[A]-[E]` `LookDiag` probes have been removed.
- The temporary `InputDiag` probe was removed after runtime evidence was
  captured.

## Acceptance Bar

- A clear root cause for the frozen on-screen view (or confirmation that the two
  fixes already resolved it and only orientation/scene-sparsity remained), plus
  a concrete fix or precise findings.
- Confirm on a display that look + WASD visibly move the view and the two boxes
  can be brought into view (turn toward +X / yaw ~90).

## Validation

```sh
cmake --build --preset debug --target ahamkara_client
ctest --test-dir build/debug -R "ahamkara_level_render_tests|ahamkara_window_input_provider_tests" --output-on-failure
./scripts/start.sh local -- --level assets/compiled/levels/prototype_box.aelevel
```

Runtime/visual confirmation requires a GL display.

## Reporting Required

Standard: write a report in `docs/reports/subagents/`, append
`docs/reports/subagents/subagent-master-log.md`, update this task `report:` and
status, and move to `review-needed/` or `blocked/`.

## Notes For Codex

First confirm the reoriented `prototype_box` spawn: bright orange debug boxes
should be visible or near-visible immediately after Play. If movement/look still
appear frozen despite the runtime evidence above, the GL present/draw layer is
the prime suspect.
