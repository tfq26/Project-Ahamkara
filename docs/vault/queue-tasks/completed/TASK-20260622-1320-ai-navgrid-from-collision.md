---
type: opencode-task
status: complete
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
report: ../../../reports/subagents/TASK-20260622-1320-ai-navgrid-from-collision-report.md
review: ../../../reports/subagents/TASK-20260622-1330-milestone-review-ai-nav-and-movement-report.md
---

# TASK-20260622-1320-ai-navgrid-from-collision

## Goal

Third AI slice (Phase 8): build a `NavGrid` from world-space collision
rectangles (so a caller can rasterize a level's `LevelCollisionBox` list into a
nav grid). Decoupled (takes plain AABBs, not `LevelAsset`, to avoid a game→render
dependency). Headless-testable.

## Scope

In bounds:
- Add `NavAABB` + `build_nav_grid(width, height, cell_size, origin_x, origin_z,
  blockers)` to `ai/nav_grid.h`: a cell is blocked when its center lies inside
  any blocker (grid Y ↔ world Z). Pure.
- Tests folded into `ahamkara_nav_grid_tests`.

Out of bounds:
- Coupling to `LevelAsset`/render types; sub-cell/partial coverage; dynamic
  rebuild — later.

## Acceptance Bar

- Empty blockers → all walkable; a partial wall blocks the right cells and
  find_path detours around it; build is deterministic. Build + tests green.

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Reporting

Self-validate on green; batch for the later Codex review.
