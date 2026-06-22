---
type: opencode-task
status: open
created: 2026-06-22
queued_by: opencode
assigned_to: codex
priority: normal
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - game
related_feature:
report:
---

# TASK-20260622-1330-milestone-review-ai-nav-and-movement

## Type

Batched milestone review (per the "self-validate, review after the fact"
workflow). One Codex pass over a milestone of self-validated work — please
accept/revise the batch holistically rather than per-item.

## In This Batch (all self-validated; build debug + tests green)

1. `TASK-20260622-1020-deterministic-character-controller` — wired the
   `game.player_*` ConfigVars into `world.cpp`'s runtime movement (walk/sprint/
   jump/gravity) via `game_module` accessors; ConfigVar defaults aligned to the
   prior constants (behavior-preserving) so movement tuning is hot-reloadable.
   + `test_movement_config_wiring`.
   Report: `reports/subagents/TASK-20260622-1020-deterministic-character-controller-impl-report.md`
2. `TASK-20260622-1300-ai-navgrid-astar` — header-only `NavGrid` + deterministic
   A* (4/8-connectivity, no corner-cutting). + `ahamkara_nav_grid_tests`.
3. `TASK-20260622-1310-ai-path-follower` — `PathFollower` + `grid_path_to_waypoints`.
4. `TASK-20260622-1320-ai-navgrid-from-collision` — `NavAABB` + `build_nav_grid`
   (rasterize collision rects into a grid; decoupled from `LevelAsset`).
   2–4 are the AI navigation foundation; all tests in `ahamkara_nav_grid_tests`.

## Validation (run by worker)

`cmake --build --preset debug` clean; `./scripts/run-tests.sh --preset debug` →
12/12. The AI nav code is pure/deterministic and fully headless-validated. The
movement-config note: sim reads global ConfigVars, so client/server must share
config for prediction determinism (standard constraint; fine for local play).

## Ask

Review the four reports + diffs, accept (move to `completed/`) or flag revisions.
The self-validated task files currently sit in `claimed/` awaiting this review.

## Notes

This is separate from `TASK-20260622-1200-phase4-netcode-milestone-review`
(the netcode milestone + the live-loop wiring that needs socket runtime).
