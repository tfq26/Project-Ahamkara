---
type: opencode-task
status: complete
created: 2026-06-15
queued_by: codex
assigned_to: opencode
priority: high
subsystems:
  - engine/render
  - client
related_feature: ../../features/2026-06-15-client-runtime-cleanup.md
report: ../../../reports/subagents/TASK-20260615-1215-render-present-semantics-report.md
---
# TASK-20260615-1215-render-present-semantics

## Goal

Clarify the distinction between rendering and presentation so renderer APIs do
not keep implying that `render()` may also swap buffers.

## Background

This task follows the client frame pipeline extraction. The cleanup inventory
calls out ambiguous presentation ownership in `DebugRenderer::render()`,
`present()`, and backend `end_frame()` semantics.

## First Read

- [Docs index](../../../README.md)
- [Agent handoff](../../../guides/agent-handoff.md)
- [Rendering map](../../systems/rendering-map.md)
- [Build and test map](../../systems/build-and-test-map.md)
- [OpenCode task queue workflow](../../workflows/opencode-task-queue.md)
- [Client Runtime Cleanup Push](../../features/2026-06-15-client-runtime-cleanup.md)

## Scope

In bounds:

- clarify staged render vs present semantics
- tighten naming/comments/API flow where safe
- keep behavior stable while making misuse harder

Out of bounds:

- full renderer rename
- broad backend rewrite
- full render graph implementation

## Likely Files

- `engine/render/include/ae/render/debug_renderer.h`
- `engine/render/include/ae/render/render_backend.h`
- `engine/render/src/debug_renderer.cpp`
- `engine/render/src/render_backend_opengl.cpp`
- `client/src/debug_client.cpp`

## Implementation Plan

1. Inspect current renderer and backend calls for where world render ends and
   buffer presentation begins.
2. Clarify or stage the API so presentation is explicit and not implied by
   generic render entry points.
3. Update call sites and comments to match the chosen semantics.
4. Keep the change narrow and report any remaining awkward boundaries.

## Acceptance Bar

- Present/swap semantics are clearer after the change.
- Call sites make render vs present ordering easier to read.
- The change stays scoped and does not attempt larger renderer modernization.

## Validation

Run when relevant:

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

If runtime render verification is needed, name the exact local command used.

## Reporting Required

When done or blocked:

1. Write a report in `docs/reports/subagents/` using
   `docs/vault/templates/subagent-report-template.md`.
2. Append `docs/reports/subagents/subagent-master-log.md`.
3. Update this task status and `report:` frontmatter.
4. Move or copy this task to `docs/vault/queue-tasks/review-needed/` or
   `docs/vault/queue-tasks/blocked/`.

## Notes For Codex Review

Codex should verify that the API clarity improved materially, not just via
comment churn.
