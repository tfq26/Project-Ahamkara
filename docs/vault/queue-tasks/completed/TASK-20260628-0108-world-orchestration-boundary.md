---
type: opencode-task
status: completed
created: 2026-06-28
queued_by: codex
assigned_to: opencode
priority: normal
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - game
  - docs
related_feature: features/2026-06-28-engine-assessment.md
report: reports/subagents/TASK-20260628-0108-world-orchestration-boundary-report.md
---

# TASK-20260628-0108-world-orchestration-boundary

## Goal

Tighten `World`'s ownership boundary so it stays an orchestrator for match
state and simulation, while player-specific state remains in `Player` and other
domain-specific systems.

## Background

The roadmap and recent refactors point to a clearer ownership model:
`World` should coordinate simulation, respawn, projectiles, dummies, and replay
history, but it should not accumulate player inventory, armor, or presentation
state. This task makes that boundary explicit and documented.

## First Read

- [Docs index](../../README.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Roadmap (consolidated)](../../roadmap/roadmap.md)
- [Engine assessment](../../features/2026-06-28-engine-assessment.md)
- [Current state](../../memory/current-state.md)

## Scope

In bounds:

- Audit `World` for remaining player-specific ownership leaks and remove them.
- Keep these responsibilities in `World`:
  - match timers and phase/state
  - scores, deaths, kill counts, and win conditions
  - respawn timing and respawn orchestration
  - dummy/projectile/collision orchestration
  - historical snapshots / rollback capture
- Push any leftover player-local state into `Player` or a narrower subsystem.
- Add/refresh docs that describe the ownership split.

Out of bounds:

- Reworking movement or weapon presentation details.
- Changing gameplay tuning or match rules.
- Broad architectural rewrites outside the boundary cleanup itself.
- Moving match stats, quests, buffs/debuffs, or progression into `Player`.
- Turning `World` into a thin shell that no longer owns sim orchestration.

## Likely Files

- `game/include/ahamkara/game/world.h`
- `game/src/world.cpp`
- `game/include/ahamkara/game/player.h`
- `docs/roadmap/roadmap.md`
- `docs/vault/memory/current-state.md`

## Implementation Plan

1. Review `World` fields and methods against the ownership model.
2. For each field/method, decide whether it belongs to `World`, `Player`, or a
   narrower runtime/controller.
3. Move only the player-local pieces; leave match/session orchestration in
   `World`.
4. Update comments/docs so the boundary is explicit and future agents can see
   the split without re-deriving it.
5. Keep build and gameplay tests green.

## Expected Shape

- `World` should remain the simulation coordinator.
- `Player` should own player identity/runtime state.
- Movement and presentation should be owned by their own layers, not hidden in
  `World` by convenience.
- If a piece of state exists because of the match loop rather than because of
  the player, it stays in `World`.

## Must Preserve

- Current match-state and respawn behavior.
- Current dummy/projectile/collision orchestration.
- Current history/rollback snapshot behavior.
- Current scoreboard / kill / death tracking.
- Current build and gameplay test status.

## Must Not Touch

- Movement feel refactors, camera refactors, or weapon presentation refactors.
- Quest, buff/debuff, or progression systems beyond documenting that they do not
  belong in `World`.
- ECS migration unless a field is being moved for ownership reasons already.
- Rendering code except where a doc comment or accessor split requires it.

## Acceptance Bar

- `World` is clearly an orchestrator, not a grab bag of player state.
- Player-specific state is owned by `Player` or a narrower runtime object.
- Documentation matches the code.
- There are no new player-specific fields added back into `World` during the
  cleanup.

## Review Tier

- `low` - primary reviewer signoff only

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Reporting Required

When done or blocked:

1. Write a report in `docs/reports/subagents/` using the report template.
2. Append `docs/reports/subagents/subagent-master-log.md`.
3. Update this task status and `report: reports/subagents/TASK-20260628-0108-world-orchestration-boundary-report.md` frontmatter.
4. Move or copy this task to `docs/vault/queue-tasks/review-needed/` or
   `docs/vault/queue-tasks/blocked/`.

## Notes For Codex Review

The review should confirm that `World` still owns orchestration, not gameplay
identity or presentation state.

## Completion Note

- Restored the prior respawn/restart weapon-runtime reset behavior and
  preserved the old 150-round reserve ammo refill.
- Switched damage-feedback numbers to post-armor health damage so combat UI
  stays consistent with actual health loss.
- Added regression tests covering respawn/restart weapon reset and actual
  damage feedback. Build + test suite pass.
