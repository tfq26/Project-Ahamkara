---
type: opencode-task
status: completed
created: 2026-06-20
queued_by: codex
assigned_to: opencode
priority: high
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - engine/render
  - tools
related_feature:
report: ../../../reports/subagents/TASK-20260620-1330-pbr-uv-plumbing-report.md
review: ../../../reports/subagents/TASK-20260620-1330-pbr-uv-plumbing-codex-review.md
---

# TASK-20260620-1330-pbr-uv-plumbing

## Goal

Plumb texture coordinates (UVs) end to end — glTF import → `.aemesh` →
`GpuMesh` → PBR shaders — so material albedo/ORM textures sample at real UVs
instead of the current constant `vec2(0)`. This makes the texture path wired by
TASK-20260620-1200 actually produce textured output.

## Background

- `GltfMesh` (engine/render/include/ae/render/gltf_loader.h) has positions,
  normals, joints, weights, indices — but **no UVs**. The loader explicitly
  parses only POSITION and NORMAL.
- `GpuMesh` (render_backend.h) has no texcoord VBO; `create_gpu_mesh`
  (render_backend_opengl.cpp) uploads pos/normal/joints/weights only.
- The PBR fragment shader samples `uAlbedoMap/uOrmMap` at `vec2(0)`
  (pbr_renderer.cpp:97-99); the vertex shader has no UV attribute/varying.
- `.aemesh` save/load (compiled_mesh.cpp) does not serialize UVs (the asset
  pipeline roundtrip test does not compare UVs).

## First Read

- [Docs index](../../../README.md)
- [Asset pipeline](../../systems/asset_pipeline.md)
- [Renderer backend](../../systems/renderer_backend.md)
- [Level world meshes report](../../../reports/subagents/TASK-20260620-1200-level-driven-world-meshes-report.md)
- [OpenCode standing instructions](../opencode-standing-instructions.md)

## Scope

In bounds:

- Add `std::vector<float> uvs;` (interleaved uv) to `GltfMesh`.
- Parse `TEXCOORD_0` in the glTF loader (gltf_loader.cpp); leave empty when absent.
- Serialize/deserialize UVs in `.aemesh` (compiled_mesh.cpp); bump
  `CompiledMeshFormat::version` and keep backward-compatible load of v1 files
  (treat missing UVs as empty).
- Add `BufferHandle vbo_texcoords;` to `GpuMesh`; upload UVs in `create_gpu_mesh`;
  free in `destroy_gpu_mesh`.
- PBR vertex shader: add `layout(location = 4) in vec2 aTexCoord;` → `out vec2 vUV`.
  PBR fragment shader: sample `uAlbedoMap`/`uOrmMap` at `vUV`. Bind attribute 4
  in `PbrRenderer::submit` (and disable after), guarded by whether the mesh has UVs.
- Importer: ensure `ahamkara_asset_importer` writes UVs through the updated
  `save_compiled_mesh` (it should be automatic if it uses the shared helper).

Out of bounds (follow-ups):

- Normal mapping / tangent basis (needs TANGENT or computed tangents) — albedo
  and ORM only here; keep `uHasNormalMap` path but do not require correct tangents.
- Texture filtering/mipmaps/sRGB sampler work (separate texture-pipeline task).
- Per-material UV transforms / multiple UV sets.

## Likely Files

- `engine/render/include/ae/render/gltf_loader.h`, `src/gltf_loader.cpp`
- `engine/render/src/compiled_mesh.cpp`
- `engine/render/include/ae/render/render_backend.h`, `src/render_backend_opengl.cpp`
- `engine/render/src/pbr_renderer.cpp`
- `tools/` importer (only if it does not already share `save_compiled_mesh`)
- `tests/src/asset_pipeline_tests.cpp` (extend mesh roundtrip to cover UVs)

## Implementation Plan

1. Add `uvs` to `GltfMesh`; parse `TEXCOORD_0` in the loader.
2. Extend `.aemesh` format (version bump + back-compat read) to store UVs; update
   the roundtrip test to assert UV equality.
3. Add `vbo_texcoords` to `GpuMesh`; upload/free in the OpenGL backend.
4. Update PBR vert/frag shaders + `submit()` attribute binding to use real UVs.
5. Verify a textured glTF (with TEXCOORD_0) imports and the PBR path binds UVs.

## Acceptance Bar

- A glTF mesh with `TEXCOORD_0` carries UVs through `.aemesh` into the `GpuMesh`.
- PBR albedo/ORM textures sample at real UVs (no more `vec2(0)`).
- v1 `.aemesh` files still load (missing UVs treated as empty; no crash).
- Build passes; existing tests stay green; mesh roundtrip test covers UVs.

## Review Tier

- `low` - additive attribute plumbing; primary reviewer signoff.

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

Runtime-visual confirmation (textured mesh) is not automatable; if attempted,
name the exact command and report it separately. Follow claim hygiene.

## Reporting Required

1. Write a report in `docs/reports/subagents/` using the report template.
2. Append `docs/reports/subagents/subagent-master-log.md`.
3. Update this task status and `report:` frontmatter.
4. Move this task to `review-needed/` or `blocked/`.

## Notes For Codex Review

Confirm `.aemesh` back-compat, that UVs reach the shader, and that the
non-UV (procedural humanoid) path still renders unaffected.

## Codex Review Outcome

Decision: `complete`

Review note:

- [TASK-20260620-1330-pbr-uv-plumbing-codex-review.md](../../../reports/subagents/TASK-20260620-1330-pbr-uv-plumbing-codex-review.md)

Required next actions:

Accepted by Codex with known follow-ups:

1. Runtime textured output still needs visual confirmation once authored content exists.
2. Tangents/normal-map correctness remains out of scope for this task.
3. A dedicated committed-v1 `.aemesh` back-compat fixture would still be useful.
