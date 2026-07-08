---
type: opencode-task
status: complete
created: 2026-06-20
queued_by: codex
assigned_to: opencode
priority: normal
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - tools
  - assets
related_feature:
report: ../../../reports/subagents/TASK-20260620-1415-blender-headless-level-generator-report.md
review: ../../../reports/subagents/TASK-20260620-1415-blender-headless-level-generator-codex-review.md
---

# TASK-20260620-1415-blender-headless-level-generator

## Goal

A headless Blender (`bpy`) generator that consumes the SAME JSON level spec from
TASK-20260620-1400 and emits blockout geometry as glTF plus a `.lvl` and a saved
`.blend` — runnable agentically via `blender -b -P build_level.py -- spec.json out/`
(Path B). This is the richer-geometry, human-editable authoring path.

## Background

The authoring-stack decision unifies Path A (`spec -> .lvl`) and Path B
(`spec -> Blender -> glTF + .lvl + .blend`) under one spec. This task adds Path B.
It enables describe -> spec -> Blender-generate -> import -> load, and produces a
`.blend` a human (or Blender-savvy collaborator) can refine. Generation is
one-way: the spec owns layout/semantics; Blender owns geometry detail.

NOTE: Blender is not installed in the current dev environment. The script can be
authored and unit-tested for its pure (non-`bpy`) parts here, but full execution
requires a Blender install on the runner.

## First Read

- [Docs index](../../../README.md)
- [Asset pipeline](../../systems/asset_pipeline.md)
- [Decision log: authoring stack](../../memory/decision-log.md)
- [Path A: level spec + emitter](TASK-20260620-1400-level-spec-and-lvl-emitter.md)
- [OpenCode standing instructions](../opencode-standing-instructions.md)

## Dependencies

- Requires the JSON level spec from TASK-20260620-1400 to exist (shared contract).

## Scope

In bounds:

- `tools/blender/build_level.py`: reads the shared JSON spec and, using `bpy`:
  builds blockout geometry (boxes/ramps/platforms/cover), places mesh instances,
  exports glTF (Blender glTF 2.0 exporter), saves a `.blend`, and writes the same
  `.lvl` the Path A emitter produces (reuse the Path A emitter as a library so
  both paths share one `.lvl` writer).
- Headless invocation contract documented: `blender -b -P tools/blender/build_level.py -- <spec.json> <out_dir>`,
  including the required Blender version and that it needs a Blender install.
- Separate the pure spec-loading/`.lvl`-writing logic from `bpy` calls so it is
  unit-testable without Blender; add a unit test for that layer.
- Doc updates describing the Path B command and how it fits the pipeline.

Out of bounds:

- Round-tripping `.blend` edits back into the spec.
- Detailed art, UV unwrapping, baked lighting, materials authoring.
- An MCP/live-control integration (can be a later, separate task).

## Likely Files

- `tools/blender/build_level.py` (new)
- shared `.lvl` writer from `tools/levelgen/` (reused)
- `tools/blender/tests/` or `tests/` pure-Python unit test (no `bpy`)
- `docs/systems/asset_pipeline.md` (document Path B)

## Implementation Plan

1. Factor the Path A `.lvl` writer into an importable module both paths use.
2. Write `build_level.py`: parse spec -> build `bpy` geometry + instances ->
   export glTF -> save `.blend` -> write `.lvl`.
3. Keep all `bpy`-free logic in a separate module; unit-test it.
4. Document the headless command, Blender version, and install requirement.

## Acceptance Bar

- Running the documented `blender -b -P` command on a spec produces glTF + `.lvl`
  + `.blend` whose `.lvl` matches the Path A output for the same spec.
- The `bpy`-free logic is unit-tested and passes without Blender installed.
- Build and existing engine tests stay green.

## Review Tier

- `low` - tooling; primary reviewer signoff.

## Validation

Automatable here (no Blender):

```sh
# pure-Python unit test for the bpy-free spec/.lvl logic
python3 -m pytest tools/blender/tests   # or the project's chosen runner
```

Blender-dependent (run on a machine with Blender installed; report the result):

```sh
blender -b -P tools/blender/build_level.py -- assets/levels/<spec>.json out/
```

If Blender execution is not run, say so explicitly. Follow claim hygiene
(separate authored / unit-tested / Blender-executed).

## Reporting Required

1. Write a report in `docs/reports/subagents/` using the report template.
2. Append `docs/reports/subagents/subagent-master-log.md`.
3. Update this task status and `report:` frontmatter.
4. Move this task to `review-needed/` or `blocked/`.

## Notes For Codex Review

Confirm both paths share one `.lvl` writer, the `bpy`-free layer is genuinely
testable without Blender, and the headless contract is documented for agentic use.
