---
type: opencode-task
task_type: child
parent: TASK-20260623-1600-deep-logging-epic.md
status: open
created: 2026-06-23
queued_by: user
assigned_to: opencode
priority: high
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - engine/core
related_feature:
report:
---

# TASK-20260623-1601-deep-logging-core-foundation

## Goal

Foundation for the deep-logging epic: extend `ae/core/log.h`/`log.cpp` with
`Debug` and `Trace` levels and runtime, per-category gating so deep logging can
be enabled selectively without flooding normal runs — then instrument the
`engine/core` component itself (category `Core`). **All other deep-logging
children depend on this task.**

## Background

See parent: [TASK-20260623-1600-deep-logging-epic](TASK-20260623-1600-deep-logging-epic.md)
and the **Shared Logging Standard** there. Today `log.h` only has
Info/Warning/Error (categorized) with no level filtering.

## First Read

- Parent epic + Shared Logging Standard
- `engine/core/include/ae/core/log.h`, `engine/core/src/log.cpp`
- [OpenCode standing instructions](opencode-standing-instructions.md)

## Scope

In bounds:

- Add `log_debug_cat` and `log_trace_cat` (categorized) to `log.h`/`log.cpp`.
- Add a runtime gate: a global minimum level + per-category enable/min-level,
  configurable via env var (e.g. `AE_LOG=Render:debug,Network:trace` and/or a
  global `AE_LOG_LEVEL`) and/or an existing ConfigVar mechanism if present.
- Ensure disabled levels are near-zero cost: the public API must check the
  effective level **before** formatting/allocating the message (provide a
  `log_enabled(category, level)` check or macro wrappers).
- Instrument `engine/core` (allocators, math/util init, config, file IO,
  ConfigVar registration, etc.) under category `Core` per the standard.
- Document the logging conventions briefly (in the asset/architecture docs or a
  short `docs/systems/` note) so component children follow one pattern.

Out of bounds:

- Instrumenting other components (their own child tasks).
- A full log sink/file-rotation/async-logging system (a later task if wanted).

## Likely Files

- `engine/core/include/ae/core/log.h`
- `engine/core/src/log.cpp`
- `engine/core/src/*` (core instrumentation)
- a short logging-conventions doc under `docs/systems/`

## Implementation Plan

1. Add Debug/Trace level enums + `log_debug_cat`/`log_trace_cat`.
2. Add the runtime gate (env var parse + per-category levels) and a cheap
   `log_enabled(...)` fast-path; wire it into all `*_cat` emitters.
3. Instrument `engine/core` lifecycle/IO/error paths under `Core`.
4. Document the conventions + how to enable per-category verbosity.

## Acceptance Bar

- New Debug/Trace categorized APIs exist and are gated at runtime.
- With deep logging off (default), there is no new output and negligible cost
  (level checked before message construction).
- With a category enabled, its Debug/Trace lines appear.
- `engine/core` emits meaningful `Core` logs at the right levels.
- Build (`debug-headless` + `debug`) clean; existing tests green; add a small
  unit test for the level/category gating if practical.

## Review Tier

- `high` - this is the shared foundation; gate the API + perf carefully.

## Validation

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

## Reporting Required

Standard: report, master log, update status/`report:`, move to `review-needed/`
or `blocked/`. Call out the chosen gating mechanism so component children reuse
it.
