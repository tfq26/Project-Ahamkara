---
type: subagent-report
category: implementation
status: validated_with_known_gaps
created: 2026-06-20
agent: opencode
subsystems:
  - tools
  - assets
branch: (working tree, uncommitted)
validation:
  - "python3 tools/levelgen/spec_to_lvl.py --selftest <spec>"
  - "ahamkara_asset_importer --manifest assets/manifest.assets"
  - "cmake --build --preset debug"
  - "./scripts/run-tests.sh --preset debug"
---

# Subagent Report

## Task

Define a canonical JSON level spec + a `spec -> .lvl` emitter (Path A) and ship
prototype levels compiled to `.aelevel`, including a mesh instance so the
level-mesh render path can be runtime-confirmed. Task:
`docs/vault/queue-tasks/claimed/TASK-20260620-1400-level-spec-and-lvl-emitter.md`.

## Status

validated_with_known_gaps

## Scope

In bounds (done): JSON spec schema + `tools/levelgen/spec_to_lvl.py`
(emit + parse-back + `--selftest`); two example specs (movement/collision arena
and a `test_box` mesh showcase); generated `.lvl`; manifest entries; importer
compiles them to `.aelevel`; spec documented in the asset pipeline doc.

Out of bounds (untouched): Blender/glTF generation (Path B), new `.lvl` fields,
in-engine editor, asset-registry-based mesh id resolution.

## Files Changed

- `tools/levelgen/spec_to_lvl.py` (new) — emitter + `parse_lvl` + `--selftest`
- `assets/levels/prototype_arena.json` / `.lvl` (new)
- `assets/levels/prototype_box.json` / `.lvl` (new)
- `assets/manifest.assets` (+2 level entries)
- `assets/compiled/levels/prototype_arena.aelevel` / `prototype_box.aelevel`
  (generated) + updated `asset_registry.tsv` / `assets.pkg`
- `docs/systems/asset_pipeline.md` (JSON spec + emitter section)

## What Changed

- Levels can now be authored as JSON and converted to the existing `.lvl` format
  with a deterministic emitter; the same spec is the shared contract for Path B.
- Two prototype levels exist; `prototype_box` contains two `test_box.aemesh`
  mesh instances that exercise the PBR level-mesh path.

## Validation Run

```sh
python3 tools/levelgen/spec_to_lvl.py --selftest assets/levels/prototype_arena.json
python3 tools/levelgen/spec_to_lvl.py --selftest assets/levels/prototype_box.json
cmake --build --preset debug --target ahamkara_asset_importer
./build/debug/tools/ahamkara_asset_importer --manifest assets/manifest.assets
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Validation Results

- Implemented: yes.
- Self-test: both specs pass `--selftest` (emit -> parse-back roundtrip).
- Importer: `Imported: 4, skipped: 0, failed: 0`; both new `.aelevel` produced.
- Build-validated: yes — full `debug` build clean (only pre-existing warnings).
- Test-validated: yes — 10/10 ctest pass (incl. asset-pipeline tests that run
  the importer).
- Runtime-confirmed: NO. No GL window run here, so the acceptance-bar item
  "prototype level loads with a visible mesh instance (runtime-confirmed)" is
  NOT met by me. Manual step below.

## Known Gaps

- Runtime visual confirmation outstanding: load
  `assets/compiled/levels/prototype_box.aelevel` via the client `level_path`
  (e.g. `./scripts/start.sh local`) and verify the two boxes render in the world
  (and that, per the prior fix, they do not overwrite the HUD). The client loads
  the compiled `.aelevel`, not the `.lvl` source.
- Mesh ids use literal paths (`assets/compiled/models/test_box.aemesh`) because
  runtime resolution is path-first; registry-based resolution is a follow-up.
- The mesh is untextured (no material/UVs authored) — it renders with scalar PBR
  defaults; textured output depends on authored materials + the UV path.

## Runtime Risks

- If the client is run from a directory other than the repo root, the literal
  mesh path will not resolve and the boxes will silently not render.

## Cross-Agent Dependencies

- The JSON spec schema + `spec_to_lvl.py` `.lvl` writer are the shared contract
  Path B (TASK-20260620-1415) must reuse.

## Recommended Next Step

Run `./scripts/start.sh local` against `prototype_box` to runtime-confirm the
boxes render (closing the TASK-20260620-1200 runtime gap), then start Path B.

## Confidence

medium-high — emitter, importer, build, and tests are all green; only the
in-window visual confirmation is outstanding (environment has no GL display).
