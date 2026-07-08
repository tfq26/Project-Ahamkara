---
type: opencode-task
status: completed
created: 2026-06-15
queued_by: codex
assigned_to: opencode
priority: high
revision: 2
review: ../../../reports/subagents/TASK-20260615-1230-pause-menu-state-owner-codex-review.md
subsystems:
  - client
  - engine/runtime
related_feature: ../../features/2026-06-15-client-runtime-cleanup.md
report: ../../../reports/subagents/TASK-20260615-1230-pause-menu-state-owner-report.md
---

# TASK-20260615-1230-pause-menu-state-owner

## Goal

Establish a clearer single owner for pause/menu state so it is not scattered
across UI controller, simulation, match state, and the client loop.

## Background

The cleanup inventory calls out pause/menu state as one of the most confusing
shared ownership areas in the local client path.

## First Read

- [Docs index](../../../README.md)
- [Agent handoff](../../../guides/agent-handoff.md)
- [Gameplay world map](../../systems/gameplay-world-map.md)
- [Build and test map](../../systems/build-and-test-map.md)
- [Client Runtime Cleanup Push](../../features/2026-06-15-client-runtime-cleanup.md)

## Scope

In bounds:

- identify the current pause/menu ownership path
- centralize ownership in one clearer client-side state owner if feasible
- reduce scattered booleans or duplicated transitions where safe

Out of bounds:

- full game session state redesign
- full menu architecture rewrite
- broad match flow redesign

## Likely Files

- `client/src/debug_client.cpp`
- `client/src/debug_ui_controller.cpp`
- `client/include/ahamkara/client/debug_ui_controller.h`
- `client/src/local_play.cpp`
- related menu/UI files touched by pause ownership

## Implementation Plan

1. Trace where pause/menu visibility and transitions are currently decided.
2. Pick one clearer owner or coordination path for that state.
3. Update the local client flow to route through that owner.
4. Leave explicit notes in the report about what still remains split.

## Acceptance Bar

- Pause/menu ownership is clearer than before.
- The client loop relies less on scattered pause/menu conditionals.
- The slice remains focused and does not attempt the whole session-state
  cleanup.

## Validation

Run when relevant:

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

If local runtime behavior is checked, name the exact run command and observed
pause/menu behavior.

## Reporting Required

When done or blocked:

1. Write a report in `docs/reports/subagents/` using
   `docs/vault/templates/subagent-report-template.md`.
2. Append `docs/reports/subagents/subagent-master-log.md`.
3. Update this task status and `report:` frontmatter.
4. Move or copy this task to `docs/vault/queue-tasks/review-needed/` or
   `docs/vault/queue-tasks/blocked/`.

## Notes For Codex Review

Codex should check whether there is genuinely a more central owner now, not just
renamed flags.

## Codex Review Outcome

Decision: `complete`

Review note:

- [TASK-20260615-1230-pause-menu-state-owner-codex-review.md](../../../reports/subagents/TASK-20260615-1230-pause-menu-state-owner-codex-review.md)

Required next actions:

Accepted by Codex with a known runtime-validation gap:

1. The architecture change is accepted.
2. Runtime menu/pause transitions still need direct observation later.
3. Headless validation remains blocked by a pre-existing dependency issue.
