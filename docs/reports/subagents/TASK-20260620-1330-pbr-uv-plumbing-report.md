---
type: subagent-report
category: implementation
status: validated_with_known_gaps
created: 2026-06-20
agent: opencode
subsystems:
  - engine/render
  - tools
branch: (working tree, uncommitted)
validation:
  - "cmake --build --preset debug"
  - "./scripts/run-tests.sh --preset debug"
---

# Subagent Report

## Task

Plumb texture coordinates (UVs) end to end — glTF import -> `.aemesh` ->
`GpuMesh` -> PBR shaders — so material albedo/ORM textures sample at real UVs
instead of `vec2(0)`. Task:
`TASK-20260620-1330-pbr-uv-plumbing` (retired local task record).

## Status

validated_with_known_gaps

## Scope

In bounds (done): UVs added to `GltfMesh`; `TEXCOORD_0` parsed in the glTF
loader; `.aemesh` format bumped to v2 with UVs appended (v1 stays a readable
prefix); `GpuMesh.vbo_texcoords` uploaded/freed in the OpenGL backend; PBR
shaders + draw call use a real `aTexCoord`/`vUV`; mesh roundtrip test extended.

Out of bounds (untouched): normal mapping / tangents, texture
filtering/mipmaps/sRGB sampler work, multiple UV sets, per-material UV transforms.

## Files Changed

- `engine/render/include/ae/render/gltf_loader.h` (`GltfMesh.uvs`)
- `engine/render/src/gltf_loader.cpp` (`TEXCOORD_0` parse + load)
- `engine/render/include/ae/render/compiled_mesh.h` (version 1 -> 2)
- `engine/render/src/compiled_mesh.cpp` (write/read UVs; accept v1+v2)
- `engine/render/include/ae/render/render_backend.h` (`GpuMesh.vbo_texcoords`)
- `engine/render/src/render_backend_opengl.cpp` (upload/free UV VBO)
- `engine/render/src/pbr_renderer.cpp` (vert `aTexCoord`->`vUV`; frag samples at
  `vUV`; bind attrib 4 in `submit`)
- `tests/src/asset_pipeline_tests.cpp` (`mesh_equal` covers UVs; new
  `test_compiled_mesh_uv_roundtrip`)

## What Changed

- glTF meshes with `TEXCOORD_0` now carry UVs through compilation into the GPU
  mesh and into the PBR shader.
- The PBR fragment shader samples albedo/ORM textures at interpolated `vUV`
  (previously a constant `vec2(0)`), so textures are no longer inert when bound.
- `.aemesh` is now v2; the loader still accepts v1 files (no UVs) by appending
  UVs at the end of each mesh record.

## Validation Run

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Validation Results

- Implemented: yes.
- Build-validated: yes. Full `debug` preset built/linked (engine, client,
  sample, tests). Only pre-existing warnings.
- Test-validated: yes. 10/10 ctest pass, including the new UV roundtrip and the
  existing `ahamkara_asset_pipeline_tests` (which recompiles meshes to v2).
- Runtime-confirmed: NO. No GL window run. Visible textured output additionally
  needs a textured asset + a level that references it (pending the authoring
  tasks); none exists yet.

## Known Gaps

- v1 back-compat is implemented (loader accepts version 1) but is only covered
  indirectly; there is no automated test that loads a committed v1 `.aemesh`
  (the v1 `assets/compiled/models/test_box.aemesh` on disk is exercised by the
  runtime level path, not by tests). Rerunning the importer regenerates it as v2.
- Normal mapping is still inert: `uHasNormalMap` exists but tangents are not
  produced, so normal maps are not correct yet (intentionally out of scope).
- No sampler/mipmap/sRGB handling changes; textures remain RGBA8 linear-ish.

## Runtime Risks

- Meshes without UVs draw with attribute 4 disabled; the PBR shader only samples
  textures when the matching `uHas*Map` flag is set, so untextured meshes are
  unaffected — but this was not visually verified in a window.

## Cross-Agent Dependencies

- `.aemesh` is now v2. Tools/agents that write compiled meshes must use the
  shared `save_compiled_mesh` (which now emits v2). Runtime reads v1 and v2.

## Recommended Next Step

Land an authoring task (Path A spec/emitter) that produces a textured mesh +
material so this path can be runtime-confirmed in a window; then schedule
tangents for correct normal mapping.

## Confidence

medium-high — build and automated tests are green and the format change is
backward-compatible by construction, but textured output has not been
runtime-confirmed in a window.
