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
report: ../../../reports/subagents/TASK-20260622-1340-ai-nav-agent-report.md
---

# TASK-20260622-1340-ai-nav-agent

## Goal

Tie the AI navigation primitives into a usable agent (roadmap Phase 8): a
`NavAgent` that plans an A* path to a world-space goal and follows it on the
fixed timestep, with the grid sourced from collision rectangles. End-to-end and
headless-testable (collision → grid → A* → waypoints → agent movement).

## Scope

In bounds:
- `game/include/ahamkara/game/ai/nav_agent.h` (header-only): `NavSpace`
  (world↔cell mapping) and `NavAgent` (`set_position`, `set_goal(world)` →
  plan path, `update(speed, dt)` → follow, `has_path`, `at_goal`, `path_cells`).
  Pure + deterministic; the owner (World) drives `update()` with the fixed dt.
- End-to-end tests folded into `ahamkara_nav_grid_tests`.

Out of bounds:
- Hooking into an actual `World` enemy entity / spawning, perception/targeting,
  combat. The `LevelCollisionBox` → `NavAABB` mapping stays at the (future) call
  site to keep the AI module free of render-layer types.

## Acceptance Bar

- Agent plans a path around a partial wall (built from collision rects), follows
  it to the goal over fixed ticks, and `at_goal()` becomes true; no-path goal
  returns false / no path. Deterministic. Build + tests green.

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Reporting

Self-validate on green; batch for the later Codex review.
