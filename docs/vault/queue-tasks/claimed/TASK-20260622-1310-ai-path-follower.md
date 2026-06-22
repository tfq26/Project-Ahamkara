---
type: opencode-task
status: self-validated
created: 2026-06-22
queued_by: opencode
assigned_to: opencode
priority: high
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - game
related_feature:
report: ../../../reports/subagents/TASK-20260622-1310-ai-path-follower-report.md
---

# TASK-20260622-1310-ai-path-follower

## Goal

Second AI slice (roadmap Phase 8): steer along a `NavGrid` path. A pure,
deterministic `PathFollower` that walks a point through world-space waypoints at
a fixed speed, plus a grid-path → world-waypoint converter. Headless-testable;
builds on `nav_grid.h`.

## Scope

In bounds:
- `game/include/ahamkara/game/ai/path_follower.h` (header-only): `NavVec2`,
  `grid_path_to_waypoints(path, cell_size, origin)` (cell centers), and
  `PathFollower` with `set_waypoints`, `advance(pos, speed, dt, arrive_radius)`,
  `finished()`, `waypoint_index()`.
- Tests folded into `ahamkara_nav_grid_tests`.

Out of bounds:
- Steering forces / acceleration smoothing, obstacle re-pathing, hooking into
  `World`/an actual agent entity — later slices.

## Acceptance Bar

- Waypoint conversion (cell centers), partial step, reaching + finishing a
  single segment, no-op advance after finish, and a multi-waypoint L-path all
  covered + passing. Deterministic. Build + tests green.

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Reporting

Self-validate on green; batch for the later Codex review (review after the fact).
