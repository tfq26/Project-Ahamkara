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
  - server
  - client
  - engine/network
related_feature:
report:
---

# TASK-20260704-1610-activity-framework

## Goal

Add the activity framework for PvE, PvP, and social session routing so services can drive activities by type.

## Background

Phase 9 sits on top of the networking core and turns it into usable matchmaking, activity, and social services. Keep the service seam clear so gameplay and session orchestration do not collapse into one file.

## First Read

- [Docs index](../../README.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Roadmap (consolidated)](../../../roadmap/roadmap.md)
- [Phase slice map](../../workflows/phase-slice-map.md)
- [Phase 4 netcode milestone review](../../queue-tasks/completed/TASK-20260622-1200-phase4-netcode-milestone-review.md)
- [Current state](../../memory/current-state.md)

## Scope

In bounds:

- Keep session/matchmaking ownership out of the gameplay loop.
- Keep service APIs explicit and testable.
- Keep the slice compatible with the existing authoritative network model.
- activity lifecycle
- session-type routing
- lobby/launch ownership

Out of bounds:

- No combat tuning.
- No rendering fidelity work.
- No world-scale streaming rewrite.
- combat tuning
- renderer work
- world-scale streaming changes

## Likely Files

  - `server/src/`
  - `client/src/`
  - `game/include/ahamkara/game/world.h`
  - `engine/network/src/`

## Implementation Plan

1. Trace the service boundary and isolate the smallest useful orchestration seam.
2. Move the target behavior into the service-facing layer while keeping gameplay adapters thin.
3. Validate the session/service transitions with the existing build/test path.

## Acceptance Bar

- Activity lifecycle is explicit and testable.
- The slice does not collapse into generic session spaghetti.
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
