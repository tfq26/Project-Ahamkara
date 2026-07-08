---
type: opencode-task
status: complete
created: 2026-06-15
queued_by: codex
assigned_to: opencode
priority: medium
subsystems:
  - client
  - engine/runtime
related_feature: ../../features/2026-06-15-client-runtime-cleanup.md
report: ../../../reports/subagents/TASK-20260615-1245-input-routing-cleanup-report.md
---

# TASK-20260615-1245-input-routing-cleanup

## Goal

Reduce overlap between gameplay input, menu input, raw polling, and UI capture
so input routing is easier to understand and less likely to double-consume
actions like ESC.

## Background

The cleanup inventory points to overlapping ownership between GLFW polling,
gameplay commands, ImGui sync, menu toggles, and controller routing.

## First Read

- [Docs index](../../../README.md)
- [Agent handoff](../../../guides/agent-handoff.md)
- [Build and test map](../../systems/build-and-test-map.md)
- [Client Runtime Cleanup Push](../../features/2026-06-15-client-runtime-cleanup.md)
- [Known traps](../../memory/known-traps.md)

## Scope

In bounds:

- clarify one input routing path for menu/gameplay coordination
- reduce duplicated handling for important menu-control actions
- improve capture rules if a clean localized fix is possible

Out of bounds:

- full input system rewrite
- full controller navigation redesign
- broad rebinding architecture work

## Likely Files

- `client/src/debug_client.cpp`
- `client/src/window_input_provider.cpp`
- `client/include/ahamkara/client/window_input_provider.h`
- `client/src/controller_bindings.cpp`
- `client/src/debug_ui_controller.cpp`

## Implementation Plan

1. Trace how one or two high-value actions, especially ESC/pause, flow through
   the current input path.
2. Remove the most confusing duplication or bypass where a safe local cleanup is
   possible.
3. Document remaining larger input-system follow-up in the report.

## Acceptance Bar

- Input routing for core menu/gameplay control is clearer.
- The change reduces at least one obvious duplicated or bypassed path.
- The slice stays localized and does not balloon into a full input rewrite.

## Validation

Run when relevant:

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

If runtime behavior is verified, name the exact command and what input behavior
was observed.

## Reporting Required

When done or blocked:

1. Write a report in `docs/reports/subagents/` using
   `docs/vault/templates/subagent-report-template.md`.
2. Append `docs/reports/subagents/subagent-master-log.md`.
3. Update this task status and `report:` frontmatter.
4. Move or copy this task to `docs/vault/queue-tasks/review-needed/` or
   `docs/vault/queue-tasks/blocked/`.

## Notes For Codex Review

Codex should verify that the chosen cleanup improves routing clarity rather than
just moving the same ambiguity elsewhere.
