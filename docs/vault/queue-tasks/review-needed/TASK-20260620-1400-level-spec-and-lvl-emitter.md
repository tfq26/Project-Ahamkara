---
type: opencode-task
status: review-needed
created: 2026-06-20
queued_by: codex
assigned_to: opencode
priority: high
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - tools
  - assets
related_feature:
report: ../../../reports/subagents/TASK-20260620-1400-level-spec-and-lvl-emitter-report.md
review: ../../../reports/subagents/TASK-20260620-1400-level-spec-and-lvl-emitter-codex-review.md
---

# TASK-20260620-1400-level-spec-and-lvl-emitter

## Goal

Define a canonical JSON level spec and a `spec -> .lvl` emitter (Path A), and
ship one or two greybox prototype levels generated from it. Compile them with the
existing importer and load them in the engine — including at least one mesh
instance so the level-mesh render path (TASK-20260620-1200) is runtime-confirmed.

## Background

The engine already consumes `.lvl` -> `.aelevel` (world settings, `[spawn]`,
`[collision]`, `[mesh]` sections — see asset pipeline doc). This task adds a
small, human- and agent-friendly JSON spec plus an emitter so levels can be
generated from a structured description without Blender. This is the fast
iteration loop in the authoring-stack decision.

## First Read

- [Docs index](../../../README.md)
- [Asset pipeline](../../systems/asset_pipeline.md) (the `.lvl` format)
- [Decision log: authoring stack](../../memory/decision-log.md)
- [Level world meshes report](../../../reports/subagents/TASK-20260620-1200-level-driven-world-meshes-report.md)
- [OpenCode standing instructions](../opencode-standing-instructions.md)

## Scope

In bounds:

- A JSON level spec schema (documented) covering: name + world settings
  (sky/ambient/gravity, skybox/ground material ids), spawn points (pos/yaw/team),
  collision boxes (AABB + flags + surface), and mesh instances (mesh asset id,
  optional material id, transform). This spec is the shared contract reused by
  Path B (TASK-20260620-1415).
- An emitter (e.g. `tools/levelgen/spec_to_lvl.py`) that reads a spec JSON and
  writes a valid `.lvl` matching the existing format exactly.
- One or two example specs + generated `.lvl` files under `assets/levels/`,
  including a mesh instance referencing `assets/compiled/models/test_box.aemesh`
  so the PBR level-mesh path renders something.
- Manifest entries so `ahamkara_asset_importer` compiles the new level(s) to
  `.aelevel`.
- A small pure-Python (or C++) unit/roundtrip check that the emitter output
  parses back to the same spec values (no engine/GL needed).

Out of bounds:

- Blender / glTF generation (that is Path B, TASK-20260620-1415).
- New `.lvl` format fields beyond what `.aelevel` already supports.
- An in-engine editor.

## Likely Files

- `tools/levelgen/spec_to_lvl.py` (new) or a small C++ tool
- `assets/levels/*.json` (specs) and generated `assets/levels/*.lvl`
- `assets/manifest.assets` (add level entries)
- `docs/systems/asset_pipeline.md` (document the JSON spec + emitter)

## Implementation Plan

1. Define and document the JSON spec schema (keep it 1:1 with `.lvl` capabilities).
2. Implement the emitter; verify byte-for-byte-sane `.lvl` output vs the format doc.
3. Author 1-2 prototype specs (e.g. a movement/collision arena + a mesh showcase).
4. Add manifest entries; run the importer to produce `.aelevel`.
5. Load via the client `level_path` and runtime-confirm (manual) that collision,
   spawns, and at least one mesh instance appear.

## Acceptance Bar

- A spec JSON deterministically produces a valid `.lvl` that the importer
  compiles to `.aelevel` without errors.
- At least one prototype level loads in the engine with a visible mesh instance
  (runtime-confirmed) — this also closes the runtime-confirm gap from
  TASK-20260620-1200.
- Spec + emitter documented; build and existing tests stay green.

## Review Tier

- `low` - additive tooling + assets; primary reviewer signoff.

## Validation

```sh
cmake --build --preset debug --target ahamkara_asset_importer
./build/debug/tools/ahamkara_asset_importer --manifest assets/manifest.assets
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

Runtime-confirm (manual): `./scripts/start.sh local` pointed at the new level;
report what was observed. Follow claim hygiene.

## Reporting Required

1. Write a report in `docs/reports/subagents/` using the report template.
2. Append `docs/reports/subagents/subagent-master-log.md`.
3. Update this task status and `report:` frontmatter.
4. Move this task to `review-needed/` or `blocked/`.

## Notes For Codex Review

Confirm the spec is the single shared contract for Path B, the `.lvl` output
matches the documented format, and a mesh instance actually renders.
