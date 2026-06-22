---
type: subagent-report
category: implementation
status: validated_with_known_gaps
created: 2026-06-20
agent: opencode
subsystems:
  - tools
branch: (working tree, uncommitted)
validation:
  - "python3 tools/blender/test_build_level.py"
  - "python3 -m py_compile tools/blender/build_level.py"
  - "cmake --build --preset debug"
  - "./scripts/run-tests.sh --preset debug"
---

# Subagent Report

## Task

Headless Blender generator (Path B) that consumes the shared Path A JSON level
spec and emits blockout geometry (glTF) + `.blend` + `.lvl`, runnable via
`blender -b -P`. Task:
`docs/vault/queue-tasks/claimed/TASK-20260620-1415-blender-headless-level-generator.md`.

## Status

validated_with_known_gaps

## Scope

In bounds (done): `tools/blender/build_level.py` (bpy-guarded) reusing the Path A
`.lvl` writer; pure bpy-free plan/`.lvl` logic; a bpy-free unit test; docs for
the headless command + Blender requirement.

Out of bounds (untouched): `.blend`->spec round-trip, detailed art/UV/lighting,
MCP/live control.

## Files Changed

- `tools/blender/build_level.py` (new) — bpy-guarded generator; pure
  `plan_blockout` / `write_lvl` reuse the Path A emitter.
- `tools/blender/test_build_level.py` (new) — bpy-free unit test.
- `docs/systems/asset_pipeline.md` (Path B section).

## What Changed

- A single JSON spec can now drive either Path A (`spec -> .lvl`) or Path B
  (`spec -> Blender -> .blend + glTF + .lvl`); both share one `.lvl` writer.
- The bpy-free logic (spec load, blockout plan, `.lvl` write) is importable and
  tested without Blender.

## Validation Run

```sh
python3 tools/blender/test_build_level.py
python3 -m py_compile tools/blender/build_level.py tools/levelgen/spec_to_lvl.py
python3 tools/blender/build_level.py assets/levels/prototype_box.json <tmp>   # bpy-free: writes .lvl
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Validation Results

- Implemented: yes.
- Unit test: `test_build_level passed` (plan math, shared-writer parity, real
  prototype spec -> 2 instances).
- Parity: `build_level` bpy-free run produced a `.lvl` identical to the Path A
  emitter for the same spec (ignoring the source-name comment line).
- Build-validated: yes — `cmake --build --preset debug` clean (no C++ changed).
- Test-validated: yes — 10/10 ctest pass.
- Blender-executed: NO. Blender is not installed in this environment, so the
  `blender -b -P` path (glTF + `.blend` production) was authored but not run.

## Known Gaps

- The full Blender run is unexecuted here; `.blend`/glTF output needs validation
  on a machine with Blender (4.x). The `.lvl` half is fully validated.
- Mesh instances become placement empties in the `.blend` (engine uses the
  `.lvl` mesh reference); importing actual mesh geometry into the `.blend` is a
  future enhancement.

## Runtime Risks

- bpy API specifics (operator names/args) are best-effort and unverified against
  a live Blender; first real run may need minor operator/arg fixups.

## Cross-Agent Dependencies

- Depends on the Path A spec/emitter (`tools/levelgen/spec_to_lvl.py`), which it
  imports as the shared `.lvl` writer.

## Recommended Next Step

On a machine with Blender 4.x, run the documented `blender -b -P` command against
`assets/levels/prototype_box.json` and confirm `.blend` + glTF are produced; fix
any operator/arg mismatches surfaced there.

## Confidence

medium — the bpy-free logic and `.lvl` parity are proven and green; the
Blender-dependent path is authored but unverified for lack of a Blender install.
