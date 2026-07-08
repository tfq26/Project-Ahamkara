---
type: opencode-task
status: claimed
created: 2026-06-15
queued_by: codex
assigned_to: opencode
priority: high
revision: 1
review: ../../../reports/subagents/TASK-20260615-1200-client-frame-pipeline-codex-review.md
subsystems:
  - client
  - engine/render
  - engine/runtime
related_feature: ../features/2026-06-15-client-runtime-cleanup.md
report: ../../../reports/subagents/TASK-20260615-1200-client-frame-pipeline-report.md
---
# TASK-20260615-1200-client-frame-pipeline

## Goal

Extract a clearer per-frame orchestration path for the local client so frame
ownership is easier to reason about and `debug_client.cpp` owns less implicit
ordering logic.

## Background

This task comes from the cleanup inventory captured in
[Client Runtime Cleanup Push](../../features/2026-06-15-client-runtime-cleanup.md).
The first slice should address the highest-priority client/runtime concerns
without trying to solve the entire renderer/UI architecture in one pass.

## First Read

- [Docs index](../../../README.md)
- [Agent handoff](../../../guides/agent-handoff.md)
- [Start here](../../00-start-here.md)
- [Repo map](../../01-repo-map.md)
- [Feature task workflow](../../workflows/feature-task-workflow.md)
- [OpenCode task queue workflow](../../workflows/opencode-task-queue.md)
- [Ahamkara reporting profile](../../workflows/ahamkara-reporting-profile.md)
- [Build and test map](../../systems/build-and-test-map.md)
- [Rendering map](../../systems/rendering-map.md)

## Scope

In bounds:

- introduce a `ClientFramePipeline` or equivalent orchestration type/function
- make frame stage order explicit in the local client path
- reduce `debug_client.cpp` responsibilities where that supports the pipeline
- improve naming/comments where needed to make frame order obvious

Out of bounds:

- full renderer rename from `DebugRenderer`
- full UI architecture split
- broad input system rewrite
- broad pause/menu state redesign

## Likely Files

- `client/src/debug_client.cpp`
- `client/include/ahamkara/client/`
- `client/src/debug_render_runtime.cpp`
- `client/src/local_play.cpp`
- `engine/render/include/ae/render/debug_renderer.h`
- `engine/render/src/debug_renderer.cpp`
- `tests/src/local_play_tests.cpp`

## Implementation Plan

1. Read the current local client loop and identify the actual frame stages:
   input, simulation/runtime update, scene build, world render, UI render,
   present.
2. Extract a small orchestration type or helper that makes those stages explicit
   in code.
3. Reduce `debug_client.cpp` to composition and top-level wiring where possible
   without spreading ownership to more globals or statics.
4. If presentation semantics are easy to clarify safely in this slice, tighten
   them; if not, leave a clean boundary and document the remaining follow-up.
5. Run relevant build/tests and report precisely what was and was not validated.

## Acceptance Bar

- Local client frame order is clearer and more explicit than before.
- `debug_client.cpp` no longer hides the main per-frame orchestration in a
  monolithic loop if a cleaner extracted owner is feasible.
- The change is scoped and does not attempt the whole cleanup inventory at once.
- Report clearly states remaining follow-up work around renderer/UI ownership.

## Validation

Run when relevant:

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

If the task touches client/runtime behavior that needs local verification,
include the exact runtime command used and what was observed.

## Reporting Required

When done or blocked:

1. Write a report in `docs/reports/subagents/` using
   `docs/vault/templates/subagent-report-template.md`.
2. Append `docs/reports/subagents/subagent-master-log.md`.
3. Update this task status and `report:` frontmatter.
4. Move or copy this task to `docs/vault/queue-tasks/review-needed/` or
   `docs/vault/queue-tasks/blocked/`.

## Notes For Codex Review

Codex should check whether the extracted pipeline actually clarifies ownership
instead of just moving code around. Review should focus on frame order clarity,
validation evidence, and whether the slice stayed scoped.

## Codex Review Outcome

Decision: `revise`

Review note:

- [TASK-20260615-1200-client-frame-pipeline-codex-review.md](../../../reports/subagents/TASK-20260615-1200-client-frame-pipeline-codex-review.md)

Required next actions:

1. Make `run_one_frame()` actually honor its shutdown contract.
2. Do not run later frame stages after failed event polling or explicit quit.
3. Keep one queue state per task.
