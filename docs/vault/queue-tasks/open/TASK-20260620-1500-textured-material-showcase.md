---
type: opencode-task
status: open
created: 2026-06-20
queued_by: codex
assigned_to: opencode
priority: high
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - assets
  - tools
  - engine/render
related_feature:
report:
---

# TASK-20260620-1500-textured-material-showcase

## Goal

Prove the PBR texture path end to end: author a UV-mapped mesh + a textured
material, reference it from a level, and confirm an albedo texture actually
samples on a level mesh in the engine. This closes the loop opened by the UV
plumbing (TASK-20260620-1330) and level-mesh (TASK-20260620-1200) work.

## Background

UVs now flow glTF `TEXCOORD_0` -> `.aemesh` -> `GpuMesh` -> PBR (`vUV`), and
`LevelRenderScene` already loads material textures into `TextureHandle`s. But no
authored content exercises it: `test_box.gltf` has no UVs and no `.mat`/`.tga`
is compiled, so textured output is still unproven. This task supplies that
content.

## First Read

- [Asset pipeline](../../systems/asset_pipeline.md)
- [UV plumbing report](../../../reports/subagents/TASK-20260620-1330-pbr-uv-plumbing-report.md)
- [Level world meshes report](../../../reports/subagents/TASK-20260620-1200-level-driven-world-meshes-report.md)
- [OpenCode standing instructions](../opencode-standing-instructions.md)

## Dependencies

- TASK-20260620-1200 (level meshes) and TASK-20260620-1330 (UVs) accepted/landed.

## Scope

In bounds:

- A UV-mapped textured cube glTF (with `TEXCOORD_0`) under `assets/models/`
  (or extend `test_box.gltf` to include UVs).
- A simple albedo `.tga` + a `.mat` referencing it under `assets/`.
- Manifest entries to compile `model`, `texture`, and `material`.
- A level spec (`assets/levels/*.json`) whose mesh instance references the mesh
  and the material; generate its `.lvl` with `tools/levelgen/spec_to_lvl.py`.
- Confirm the importer compiles all assets and the runtime resolves the material
  textures (path-first resolution).

Out of bounds:

- Normal/ORM maps and tangents, mipmap/sRGB sampler work, atlasing.

## Implementation Plan

1. Provide/author a UV-mapped cube glTF + a small albedo TGA + a `.mat`.
2. Add manifest entries; run the importer.
3. Author a showcase level spec referencing the textured mesh + material; emit `.lvl`.
4. Verify the material texture id resolves at runtime (literal path) and the
   `LevelRenderScene` binds the albedo map.

## Acceptance Bar

- Importer compiles model + texture + material + level with 0 failures.
- The showcase level's mesh instance carries a non-zero albedo `TextureHandle`
  at runtime (the texture path resolves and uploads).
- Build + existing tests stay green.

## Review Tier

- `low`.

## Validation

```sh
cmake --build --preset debug --target ahamkara_asset_importer
./build/debug/tools/ahamkara_asset_importer --manifest assets/manifest.assets
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

Runtime-visual confirmation (texture appears on the mesh) needs a GL display;
defer to TASK-20260620-1520 / a machine with a display, and report what was vs
was not visually confirmed. Follow claim hygiene.

## Reporting Required

Standard: write report, append master log, update task `report:`/status, move to
`review-needed/` or `blocked/`.
