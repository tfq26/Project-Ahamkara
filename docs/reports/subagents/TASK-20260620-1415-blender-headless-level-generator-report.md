---
type: subagent-report
category: implementation
status: implemented
created: 2026-06-23
agent: opencode
subsystems:
  - tools
branch: main (HEAD ca313bc)
validation:
  - "python3 tools/blender/test_build_level.py (bpy-free unit test)"
  - "blender -b -P tools/blender/build_level.py -- assets/levels/prototype_box.json <out> (Blender 5.1.2)"
  - "blender -b -P tools/blender/build_level.py -- assets/levels/prototype_arena.json <out> (Blender 5.1.2)"
  - "diff <out>/*.lvl assets/levels/<spec>.lvl -> identical (both specs)"
  - "cmake --build --preset debug + ctest (12/12)"
---

# Subagent Report

## Task

Headless Blender generator (Path B) that consumes the shared Path A JSON level
spec and emits blockout geometry (glTF) + `.blend` + `.lvl`, runnable via
`blender -b -P`. Task: `TASK-20260620-1415-blender-headless-level-generator`.

## Status

implemented — now **Blender-executed and verified** headlessly. The sole prior
blocker ("Blender not installed; `blender -b -P` path unrun") is closed. Ready
for Codex review.

## What Closed The Block

Blender is now installed (`/Applications/Blender.app`, **Blender 5.1.2**). The
documented headless command was run on two specs and produced all artifacts with
**no operator/arg fixups** (the risk the previous report flagged did not
materialize, even though the script was authored against 4.x):

```sh
blender -b -P tools/blender/build_level.py -- assets/levels/prototype_box.json   <out>
blender -b -P tools/blender/build_level.py -- assets/levels/prototype_arena.json <out>
```

Each run wrote `<name>.blend`, `<name>.gltf` (+ `.bin`, GLTF_SEPARATE), and
`<name>.lvl`.

## Validation Results

- bpy-free unit test: `test_build_level passed`.
- Blender run (5.1.2), prototype_box: produced `.blend` (86 KB) + `.gltf` + `.bin`
  + `.lvl`; glTF export reported success (1 primitive).
- Blender run (5.1.2), prototype_arena: produced `.blend` + `.gltf` + `.bin` +
  `.lvl`.
- **`.lvl` parity (acceptance bar):** the Blender-written `.lvl` is **byte-for-byte
  identical** to the committed Path A output (`diff` clean) for BOTH specs —
  proving both paths share one `.lvl` writer.
- Build/tests: `cmake --build --preset debug` clean; `ctest` 12/12 (no C++
  changed; re-confirmed on current HEAD).

## Acceptance Bar Check

- Documented `blender -b -P` produces glTF + `.lvl` + `.blend` whose `.lvl`
  matches Path A for the same spec — MET (identical, 2 specs).
- bpy-free logic unit-tested and passes without Blender — MET.
- Build and existing tests stay green — MET (12/12).

## Files Changed

None this pass — verification only. (The generator
`tools/blender/build_level.py`, its bpy-free unit test, and the asset-pipeline
docs were already implemented/committed.)

## Known Gaps

- Mesh instances become placement empties in the `.blend` (the engine uses the
  `.lvl` mesh reference); importing real mesh geometry into the `.blend` remains
  a future enhancement (out of scope).
- Generated artifacts were written to a scratch dir and not committed (Path B is
  a generator; checked-in levels come via the manifest/importer).

## Runtime Risks

- None new. Verified on Blender 5.1.2; behavior on much older/newer majors could
  differ but is not a current concern.

## Cross-Agent Dependencies

- Reuses the Path A `.lvl` writer (`tools/levelgen/spec_to_lvl.py`); parity holds.

## Recommended Next Step

Codex: review/accept. To invoke Blender on this machine, use the full binary
path: `/Applications/Blender.app/Contents/MacOS/Blender -b -P
tools/blender/build_level.py -- <spec.json> <out_dir>` (or add an alias). The
other blocked level tasks (1400/1500/1510/1520) still require a GL **display**,
which Blender does not provide.

## Confidence

high — the documented headless command runs clean and the `.lvl` output is
byte-identical to Path A on two specs; the only previously-outstanding item
(Blender execution) is now done.
