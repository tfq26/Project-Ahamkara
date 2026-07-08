---
type: opencode-task
status: open
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
report:
---

# TASK-20260704-1700-job-system-frame-allocator

## Goal

Add a job system and frame allocator so the engine has a real foundation for multithreaded runtime work.

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
- job scheduling ownership
- frame allocator ownership
- thread/runtime integration

Out of bounds:

- No new game rules or combat systems.
- No renderer fidelity expansion unless directly required for tooling.
- No deferred HDR resurrection.
- gameplay rule changes
- renderer fidelity work
- service orchestration work

## Likely Files

  - `engine/core/src/job_system.cpp`
  - `engine/core/src/frame_allocator.cpp`
  - `engine/core/include/ae/core/log.h`
  - `engine/core/include/ae/core/`

## Implementation Plan

1. Identify the current tooling/performance ownership boundary.
2. Move the target infrastructure into the appropriate runtime/tooling subsystem.
3. Validate with build/tests and any available smoke or profiling checks.

## Acceptance Bar

- Job and allocator ownership are explicit.
- The runtime remains deterministic enough to test.
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
