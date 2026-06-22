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
report: ../../../reports/subagents/TASK-20260622-1300-ai-navgrid-astar-report.md
---

# TASK-20260622-1300-ai-navgrid-astar

## Goal

First slice of FPS AI navigation (roadmap Phase 8): a uniform `NavGrid` +
deterministic A* pathfinding. Pure logic — no display, socket, or physics — so
it is fully headless-unit-testable. Foundation for later AI combatant movement.

## Verify First (done)

Grep across `game/` for navmesh/pathfind/A*/navigation/perception/behavior =
no hits. AI navigation is genuinely greenfield.

## Scope

In bounds:
- `game/include/ahamkara/game/ai/nav_grid.h` — header-only, dependency-free:
  `NavGrid` (walkable/blocked cells, bounds) + `find_path(grid, start, goal,
  allow_diagonal)` returning a cell path (empty if none). Deterministic A*
  (stable tie-break, fixed neighbor order, no diagonal corner-cutting).
- `tests/src/nav_grid_tests.cpp` + a `ahamkara_nav_grid_tests` target.

Out of bounds:
- 3D navmesh, dynamic obstacle avoidance, steering/flocking, behavior trees,
  perception, hooking AI into `World` — later slices.

## Acceptance Bar

- A*: straight path, detour around a wall, no-path, start==goal, blocked
  endpoints, and 8-connected shorter than 4-connected — all covered + passing.
- Deterministic output. Build + existing tests stay green.

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Reporting

Self-validate on green build+tests; batch into the next milestone Codex review
(not sent individually, per current workflow).
