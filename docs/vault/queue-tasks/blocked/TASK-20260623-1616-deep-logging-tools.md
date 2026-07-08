---
type: opencode-task
task_type: child
parent: TASK-20260623-1600-deep-logging-epic.md
status: blocked
created: 2026-06-23
queued_by: user
assigned_to: opencode
priority: normal
escalation_tier: low
revision: 1
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - tools
related_feature:
report: reports/subagents/TASK-20260623-1616-deep-logging-tools-report.md
review: ../../../reports/subagents/TASK-20260623-1616-deep-logging-tools-codex-review.md
---

# TASK-20260623-1616-deep-logging-tools

## Goal

Instrument `tools` (asset importer + level generators) with deep, level-gated
logging under category `Tools`, per the parent epic's Shared Logging Standard.

## Background

Parent: [TASK-20260623-1600-deep-logging-epic](TASK-20260623-1600-deep-logging-epic.md).
**Depends on** child 1601 for the C++ tools. Read the Shared Logging Standard.
NOTE: the C++ tools build under the GUI config — validate with `debug`. The
Python generators (`tools/levelgen`, `tools/blender`) should use clear,
level-prefixed prints/`logging` consistent in spirit (they don't use the C++
log facility).

## Scope

In bounds (logging only):
- Asset importer: per-asset import/skip/fail with reason, manifest parse,
  registry + package write, totals (Info; failures at Error). Make outcomes
  unambiguous (the current "Imported/skipped/failed" line is a good base).
- Level generators (`spec_to_lvl.py`, `build_level.py`): spec load, sections
  emitted, output paths, Blender steps (Info; warnings on missing/invalid).
- C++ tools: `#define AE_LOG_CATEGORY "Tools"`.

Out of bounds: behavior changes; per-asset Trace spam beyond what aids debugging.

## Likely Files

- `tools/*` C++ (asset importer), `tools/levelgen/*.py`, `tools/blender/*.py`

## Implementation Plan

1. C++ tools: add the `Tools` category + per-asset/import-step logs.
2. Python tools: consistent level-prefixed messages for load/emit/errors.

## Acceptance Bar

- `Tools` logs (C++) at correct levels; importer outcomes are clear; default run
  not spammed.
- Python generator messages are clear and consistent.
- Build clean; importer runs on `assets/manifest.assets` with 0 failures;
  existing tests (incl. `ahamkara_asset_pipeline_tests`) green.

## Review Tier

- `low`.

## Validation

```sh
cmake --build --preset debug
./build/debug/tools/ahamkara_asset_importer --manifest assets/manifest.assets
./scripts/run-tests.sh --preset debug
```

## Reporting Required

Standard: report, master log, status/`report: reports/subagents/TASK-20260623-1616-deep-logging-tools-report.md`, move to `review-needed/`.

## Codex Review Outcome

Codex found no C++ tool logging changes in the repo diff. Add real
`Tools`-category logging to the actual `tools/*.cpp` binaries in scope, then
resubmit.

## Deferred Note

This slice is deferred until the user says the project is in a working state
again. Do not treat it as active queue work before then.
