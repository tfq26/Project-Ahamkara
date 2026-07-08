---
type: opencode-task
status: completed
created: 2026-06-20
queued_by: codex
assigned_to: opencode
priority: high
escalation_tier: low
revision: 2
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - engine/render
  - client
related_feature:
report: ../../../reports/subagents/TASK-20260620-1200-level-driven-world-meshes-report.md
review: ../../../reports/subagents/TASK-20260620-1200-level-driven-world-meshes-codex-review-final.md
---

# TASK-20260620-1200-level-driven-world-meshes

## Goal

Make compiled levels actually render their authored geometry: load each
`LevelAsset::mesh_instances` entry at runtime, upload its `.aemesh` (and any
referenced `.aemat` material + `.aetex` textures) to the GPU, and draw the
instances each frame through the existing `ae::render::PbrRenderer`, wired into
the client frame pipeline. This is the first slice of the "render spaces and
maps well" effort.

## Background

Today the rendered world is 100% hardcoded procedural geometry in
`engine/render/src/map_geometry.cpp` (`build_arena()`), and the data-driven
path is fully built but dead:

- `LevelAsset::mesh_instances`, `skybox_material`, `ground_material` are loaded
  from `.aelevel` but never rendered (`client/src/local_play.cpp:215`,
  `game/src/world.cpp:543` consume only collision boxes + spawns).
- `PbrRenderer` is instantiated (`client/src/debug_client.cpp:61`) but
  `submit()` / `begin_frame()` have zero runtime callers.
- `CompiledMeshLoader`, `CompiledMaterialLoader`, `CompiledTextureLoader`, and
  `RenderBackend::create_texture()` are only exercised by asset-pipeline tests.
- `ShadowPass` already runs and produces a depth map; `PbrRenderer::begin_frame`
  already accepts a `ShadowPass*` and binds the shadow map for lighting.

`assets/levels/javelin4.lvl` currently has no `[mesh]` section and the manifest
compiles no materials/textures, so this slice must also provide a minimal,
in-memory or authored mesh instance to exercise the path.

## First Read

- [Docs index](../../../README.md)
- [Agent handoff](../../../guides/agent-handoff.md)
- [Architecture](../../systems/architecture.md)
- [Renderer backend](../../systems/renderer_backend.md)
- [Asset pipeline](../../systems/asset_pipeline.md)
- [OpenCode task queue workflow](../../workflows/opencode-task-queue.md)
- [OpenCode standing instructions](../opencode-standing-instructions.md)

## Scope

In bounds:

- A small owner type (e.g. `LevelRenderModel` / `LevelRenderScene`) that, given a
  loaded `LevelAsset` + `RenderBackend`, loads each `mesh_instance`'s `.aemesh`
  into a `GpuModel`, resolves its `.aemat` material (scalar albedo/metallic/
  roughness) and any referenced `.aetex` textures into `TextureHandle`s, and
  computes a column-major world model matrix from `pos/yaw/pitch/roll/scale`.
- Submitting those instances to `PbrRenderer` each frame with the active camera
  view/projection + camera position and the current `ShadowPass`.
- Wiring `PbrRenderer` through `ClientFramePipeline` into the world-render stage
  (recommended seam: `client/src/debug_render_runtime.cpp` between
  `shadow_pass.end_pass()` and `renderer.render(scene)`, so PBR stays decoupled
  from `DebugRenderer` internals; confirm camera matrices are available in the
  render submission and add them if not).
- Graceful fallback: missing material -> default PBR scalars; missing/absent
  textures -> untextured PBR (do not hard-fail the frame).
- Deterministic resource ownership: create on level load, destroy on
  shutdown/level change. No per-frame allocations or GPU uploads.
- A headless-friendly unit test (no GL) for the pure logic: model-matrix
  construction from a `LevelMeshInstance`, and level->draw-call assembly using an
  in-memory `LevelAsset` referencing `assets/compiled/models/test_box.aemesh`.

Out of bounds (explicit follow-ups, do NOT do here):

- Static-geometry batching / instanced draws / draw sorting.
- Replacing or generalizing the `MapGeometry` 4x4 grid / spatial partition.
- Skybox / ground-material rendering, fog, HDR, post-processing.
- Multiple/point/spot lights, CSM, mesh shadow casters.
- New texture compression / mipmaps / sampler-state work.
- Authoring a full textured `javelin4` (a single test mesh instance is enough).

## Likely Files

- `engine/render/include/ae/render/` (new `level_render.h` or similar)
- `engine/render/src/` (new `level_render.cpp`)
- `engine/render/src/pbr_renderer.cpp` / `pbr_renderer.h` (only if a real gap is
  found; avoid broad changes)
- `client/src/debug_client.cpp`
- `client/src/client_frame_pipeline.cpp` / `.h`
- `client/src/debug_render_runtime.cpp` / `.h`
- `engine/render/CMakeLists.txt`, `tests/CMakeLists.txt`
- `tests/src/` (new `level_render_tests.cpp`)

## Implementation Plan

1. Read the seam files end to end before editing: `client/src/debug_client.cpp`,
   `client/src/client_frame_pipeline.{h,cpp}`, `client/src/debug_render_runtime.{h,cpp}`,
   `engine/render/include/ae/render/pbr_renderer.h`, `compiled_level.h`,
   `compiled_material.h`, `compiled_texture.h`, `compiled_mesh.h`,
   `render_backend.h`.
2. Add a `LevelRenderScene` (engine/render) that owns loaded `GpuModel`s,
   `TextureHandle`s, and per-instance model matrices, with `build(const LevelAsset&,
   RenderBackend&, asset_root)` and `destroy(RenderBackend&)`. Resolve asset ids to
   compiled paths the same way existing loaders/registry do.
3. Add a `render(PbrRenderer&)` (or `collect(std::vector<PbrDrawCall>&)`) method that
   emits one `PbrDrawCall` per mesh, filling albedo/metallic/roughness + texture
   handles + model matrix.
4. Ensure camera view/projection + camera position are available at the chosen seam
   (extend the render submission if needed) and call
   `pbr.begin_frame(view, proj, cam_pos, &shadow); ...submit...; pbr.end_frame();`.
5. Thread `PbrRenderer&` from `debug_client.cpp` through `ClientFramePipeline` into
   the world-render stage. Build the `LevelRenderScene` once at level load
   (alongside `simulation.load_level`), destroy on shutdown.
6. Provide one test mesh instance to exercise the path (in-memory `LevelAsset` in
   the unit test referencing `test_box.aemesh`; optionally add a `[mesh]` line +
   importer entry only if cheap and non-binary).
7. Add `level_render_tests.cpp`: assert model-matrix values for a known
   pos/yaw/scale and that draw-call assembly from an in-memory `LevelAsset`
   produces the expected count and mesh pointers. Keep it GL-free.
8. Update `docs/systems/renderer_backend.md` (or asset_pipeline.md) with a short
   note that levels now drive PBR world meshes, and what is still hardcoded.

## Acceptance Bar

- A level containing a mesh instance results in that mesh being submitted to
  `PbrRenderer` each frame with a correct world transform; verified by the new
  unit test for the assembly + matrix logic.
- `PbrRenderer` is actually invoked in the runtime render path (no longer dead).
- Missing materials/textures degrade gracefully (no crash, sane defaults).
- No per-frame GPU uploads or allocations for static level meshes.
- The hardcoded `build_arena()` path is left intact and still renders (this slice
  is additive, not a replacement).
- Build passes and existing tests stay green.

## Review Tier

- `high` - this activates a previously-dead render path and touches the client
  frame pipeline; primary + secondary reviewer before completion.

## Validation

Required (automatable):

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

Note: the `debug` preset (not `debug-headless`) is required because this touches
the client + render targets. If `debug` cannot build the client in this
environment, fall back to `debug-headless` for the engine/render + tests and say
so explicitly in the report.

Runtime-visual confirmation is NOT automatable here (needs a GL window). If
attempted, name the exact command, e.g.:

```sh
./scripts/start.sh local
```

and report it separately as runtime-confirmed vs only build/test-validated.
Follow claim hygiene: separate implemented / build-validated / test-validated /
runtime-confirmed.

## Reporting Required

When done or blocked:

1. Write a report in `docs/reports/subagents/` using
   `docs/vault/templates/subagent-report-template.md`.
2. Append `docs/reports/subagents/subagent-master-log.md`.
3. Update this task `status:` and `report:` frontmatter.
4. Move or copy this task to `docs/vault/queue-tasks/review-needed/` or
   `docs/vault/queue-tasks/blocked/`.

## Notes For Codex Review

Confirm `PbrRenderer` is genuinely in the frame path (not just compiled),
that level transforms are correct (column-major, matches existing matrix
convention), resource ownership/teardown is leak-free, and that the additive
approach did not regress the existing hardcoded arena render.

## Codex Review Outcome

Decision: `complete`

Review note:

- [TASK-20260620-1200-level-driven-world-meshes-codex-review-final.md](../../../reports/subagents/TASK-20260620-1200-level-driven-world-meshes-codex-review-final.md)

Accepted after revision:

1. The PBR level-mesh pass now runs in the 3D/world phase before overlays.
2. The GL-free test now covers level mesh instance assembly and draw-call shape.
3. The revised report re-ran `cmake --build --preset debug` and
   `./scripts/run-tests.sh --preset debug` successfully.
