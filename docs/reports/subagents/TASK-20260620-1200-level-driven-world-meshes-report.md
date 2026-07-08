---
type: subagent-report
category: implementation
status: validated_with_known_gaps
created: 2026-06-20
agent: opencode
subsystems:
  - engine/render
  - client
branch: (working tree, uncommitted)
validation:
  - "cmake --build --preset debug"
  - "./scripts/run-tests.sh --preset debug"
---

# Subagent Report

## Task

Make compiled levels drive textured world geometry through the existing (but
previously dead) `ae::render::PbrRenderer`: load `LevelAsset::mesh_instances`,
upload GPU meshes + materials/textures, and submit them each frame, wired into
the client frame pipeline. Task:
`docs/vault/queue-tasks/claimed/TASK-20260620-1200-level-driven-world-meshes.md`.

## Status

validated_with_known_gaps

## Scope

In bounds (done): a `LevelRenderScene` owner in `engine/render`, scalar-PBR
draw-call assembly from materials, camera-matrix accessors on `DebugRenderer`,
PBR submission wired into `render_local_debug_frame` via the frame pipeline, and
a GL-free unit test for the transform/material logic.

Out of bounds (untouched, intentionally): static batching/instancing, spatial
partition generalization, skybox/ground-material/fog, multiple lights/CSM/mesh
shadow casters, texture compression/mipmaps, UV plumbing, registry-based asset
resolution. The hardcoded `build_arena()` path is left intact (additive change).

## Files Changed

- `engine/render/include/ae/render/level_render.h` (new)
- `engine/render/src/level_render.cpp` (new)
- `engine/render/include/ae/render/debug_renderer.h` (camera getters)
- `engine/render/src/debug_renderer.cpp` (capture matrices + getter defs)
- `client/include/ahamkara/client/debug_render_runtime.h` (signature + includes)
- `client/src/debug_render_runtime.cpp` (PBR submit after world render)
- `client/include/ahamkara/client/client_frame_pipeline.h` (ctor params + members)
- `client/src/client_frame_pipeline.cpp` (ctor + stage_render_world)
- `client/src/debug_client.cpp` (build/destroy LevelRenderScene, wiring)
- `engine/render/CMakeLists.txt` (+`level_render.cpp`)
- `tests/CMakeLists.txt` (+`ahamkara_level_render_tests`)
- `tests/src/level_render_tests.cpp` (new)

## What Changed

- `PbrRenderer` is now actually invoked at runtime (it had zero callers).
- `DebugRenderer` exposes `view_matrix()`, `projection_matrix()`,
  `camera_position()` (column-major) captured each `render()`, so the PBR pass
  aligns with the debug world.
- A `LevelRenderScene` loads each `LevelMeshInstance` (`.aemesh` -> `GpuModel`),
  resolves the optional `.aemat` material to scalar albedo/metallic/roughness,
  loads any referenced `.aetex` textures into `TextureHandle`s, and submits one
  `PbrDrawCall` per mesh with a column-major TRS world matrix.
- The client builds the scene at level load and frees it before backend shutdown.

## Validation Run

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Validation Results

- Implemented: yes.
- Build-validated: yes. Full `debug` preset (engine, client, flashback sample,
  tests) compiled and linked. Only pre-existing warnings (entt deprecated
  literal-operator; duplicate-library linker warning). No errors.
- Test-validated: yes. 10/10 ctest pass, including the new
  `ahamkara_level_render_tests` (transform + material-mapping logic).
- Runtime-confirmed: NO. No GL window was launched. Additionally the only sample
  level (`assets/levels/javelin4.lvl`) has no `[mesh]` instances, so the new path
  currently submits nothing visible at runtime — it is active but untriggered
  until a level with mesh instances is authored.

## Known Gaps

- No authored content exercises the path end-to-end visually yet (no level with
  mesh instances; no compiled material/texture in the manifest).
- Textured output is inert: the PBR fragment shader samples textures at a
  constant UV (`pbr_renderer.cpp:97-99`); UVs are not plumbed. Scalar PBR renders.
- Asset-id resolution is literal-path-first with an `asset_root` join fallback;
  `assets/compiled/asset_registry.tsv`-based resolution is deferred.
- No depth/blend state restoration after the PBR pass beyond setting depth
  test/write/lequal; fine today (PBR runs last before present) but worth noting.

## Runtime Risks

- First real runtime use needs a level with mesh instances; transform/winding
  and depth interaction with the legacy pass should be eyeballed in a GL window.
- `PbrRenderer::submit` manages its own GL attrib/state; mixing with the legacy
  fixed-function pass was not visually verified.

## Cross-Agent Dependencies

- `DebugRenderer` gained public camera getters — other render work can rely on
  them. The PBR pass now runs between `renderer.render()` and `present()` in
  `render_local_debug_frame`.

## Recommended Next Step

Author a minimal `[mesh]` instance + `.mat` (+ optional `.tga`) in the manifest,
recompile assets, and run `./scripts/start.sh local` to runtime-confirm a
textured/scalar level mesh renders aligned with the world. Then schedule UV
plumbing so material textures become meaningful.

## Confidence

medium — build and automated tests are green and the wiring is straightforward,
but the path has not been runtime-confirmed in a window and no authored level
currently triggers it.

## Revision 2026-06-20 (addresses Codex review)

Both review findings fixed:

1. Render order: the PBR level-mesh pass no longer runs after
   `DebugRenderer::render()`. `render()` now takes an optional
   `draw_world_extra` callback that it invokes after the main color pass and
   before the screen-space overlay pass. `render_local_debug_frame` passes the
   PBR submit as that callback, so level meshes draw in the 3D world phase and
   can no longer overwrite the HUD/crosshair/menu.
2. Test coverage: extracted a pure, GL-free `make_level_draw_call(instance, mesh)`
   helper and added `test_draw_call_assembly`, which asserts per-mesh draw-call
   field assembly (mesh pointer, model-matrix pointer, albedo, metallic,
   roughness, texture handles) and that an N-mesh model assembles into N draw
   calls.

Files additionally changed: `engine/render/include/ae/render/debug_renderer.h`
(+`<functional>`, `render()` callback param), `engine/render/src/debug_renderer.cpp`
(invoke callback before overlays), `engine/render/include/ae/render/level_render.h`
(+`pbr_renderer.h`, `make_level_draw_call`), `engine/render/src/level_render.cpp`
(helper + `submit` uses it), `client/src/debug_render_runtime.cpp` (callback seam),
`tests/src/level_render_tests.cpp` (assembly test).

Re-validated: `cmake --build --preset debug` clean; `./scripts/run-tests.sh
--preset debug` -> 10/10 pass. Still not runtime-confirmed in a GL window
(no authored level with mesh instances exists yet).
