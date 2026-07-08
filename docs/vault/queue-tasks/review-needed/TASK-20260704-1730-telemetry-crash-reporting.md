---
type: opencode-task
status: review-needed
created: 2026-07-04
queued_by: codex
assigned_to: opencode
priority: normal
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - engine/core
  - engine/render
  - client
  - server
  - tools
related_feature:
report: docs/reports/subagents/20260708-1730-telemetry-crash-reporting-report.md
---

# TASK-20260704-1730-telemetry-crash-reporting

## Goal

Add telemetry, crash reporting, and diagnostics capture so runtime failures become actionable.

## Background

Phase 10 is the production-readiness track: job system, frame allocator, profiling, authoring ergonomics, telemetry, crash reporting, and CI. These slices should make the engine faster and easier to work on without moving gameplay ownership around again.

## First Read

- [Docs index](../../README.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Roadmap (consolidated)](../../../roadmap/roadmap.md)
- [Phase slice map](../../workflows/phase-slice-map.md)
- [Deep logging core foundation report](../../queue-tasks/completed/TASK-20260623-1601-deep-logging-core-foundation.md)
- [Render present semantics report](../../queue-tasks/completed/TASK-20260615-1215-render-present-semantics.md)
- [UI screen split plan report](../../queue-tasks/completed/TASK-20260615-1300-ui-screen-split-plan.md)
- [Current state](../../memory/current-state.md)

## Scope

In bounds:

- Keep the tooling slice focused on infrastructure and observability.
- Keep gameplay state out of tooling ownership.
- Keep validation honest and reproducible.
- runtime telemetry
- crash reporting
- diagnostic capture path

Out of bounds:

- No new game rules or combat systems.
- No renderer fidelity expansion unless directly required for tooling.
- No deferred HDR resurrection.
- gameplay rule changes
- renderer fidelity work
- service orchestration changes

## Likely Files

  - `engine/core/src/`
  - `client/src/`
  - `server/src/`
  - `docs/vault/workflows/phase-slice-map.md`

## Implementation Plan

1. Identify the current tooling/performance ownership boundary.
2. Move the target infrastructure into the appropriate runtime/tooling subsystem.
3. Validate with build/tests and any available smoke or profiling checks.

## Acceptance Bar

- Telemetry/crash behavior is explicit.
- Diagnostics are treated as support tooling, not gameplay ownership.
- Build and tests remain green.

## Review Tier

- `low` - primary reviewer signoff only

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

If validation is skipped or fails, explain why in the report.

## Reporting Required

When done or blocked:

1. Write a report in `docs/reports/subagents/` using the report template.
2. Append `docs/reports/subagents/subagent-master-log.md`.
3. Update this task status and `report:` frontmatter.
4. Move or copy this task to `docs/vault/queue-tasks/review-needed/` or
   `docs/vault/queue-tasks/blocked/`.

## Notes For Codex Review

Confirm the slice keeps the ownership boundary explicit and does not leak
runtime authority back into the wrong subsystem.
