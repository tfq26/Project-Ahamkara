---
type: opencode-task
status: completed
created: 2026-06-28
queued_by: codex
assigned_to: opencode
priority: normal
escalation_tier: medium
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - game
  - engine/collision
related_feature: features/2026-06-28-engine-assessment.md
report: reports/subagents/2026-07-04-1424-collision-response-polish-oz.md
completed_by: oz
---

# TASK-20260628-0110-collision-response-polish

## Goal

Finish the player-facing collision response polish for step, slope, and ledge
behavior after the movement controller split.

## Background

Phase 2 is not just about having a controller. The player should feel grounded
when moving across uneven geometry, and the collision response should remain
deterministic while handling the remaining edge cases around steps, slopes, and
ledge transitions.

## First Read

- [Docs index](../../README.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Roadmap (consolidated)](../../roadmap/roadmap.md)
- [Engine assessment](../../features/2026-06-28-engine-assessment.md)
- [Current state](../../memory/current-state.md)

## Scope

In bounds:

- Review and polish the player-facing collision response for:
  - step-up and step-down behavior
  - slope handling
  - ledge / mantle / drop transitions
  - ground contact edge cases that make movement feel sticky or floaty
- Keep the response deterministic and compatible with the existing physics
  character controller path.
- Update tests or add focused coverage where the behavior can be locked in.

Out of bounds:

- Rewriting the physics backend or switching away from the current character
  controller approach.
- Changing weapon, menu, or match-state systems.
- Touching camera/viewmodel presentation except as a side effect of movement.

## Likely Files

- `game/src/world.cpp`
- `game/include/ahamkara/game/world.h`
- `game/include/ahamkara/game/movement.h`
- `game/src/player_movement_controller.cpp` if created by the movement split
- `game/src/world_jolt_bridge.cpp`

## Implementation Plan

1. Identify the current collision-response edge cases in the movement path.
2. Polish the response without altering the intended feel.
3. Keep the movement/controller seam clean if the extraction task already landed.
4. Add or update tests for the collision behavior that was fixed.

## Expected Shape

- Step/slope/ledge response is handled by the movement/controller layer, not by
  ad hoc `World` special cases.
- The solution should be small and behavioral, not a physics rewrite.
- The collision response should stay deterministic and easy to reason about.

## Must Preserve

- Current movement feel and the fixed-timestep simulation path.
- Existing ground, jump, slide, and mantle behavior except for the targeted
  polish fixes.
- Existing collision world integration.
- Current build and gameplay test stability.

## Must Not Touch

- Weapon presentation or weapon runtime.
- Menu/HUD state.
- Match timers, respawn logic, or score tracking.
- Broad collision-engine refactors outside the player response seam.

## Acceptance Bar

- Collision response feels cleaner on steps, slopes, and ledges.
- No regressions in deterministic movement behavior.
- Build passes and tests stay green.

## Review Tier

- `medium` - primary reviewer signoff plus a quick sanity pass

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Reporting Required

When done or blocked:

1. Write a report in `docs/reports/subagents/` using the report template.
2. Append `docs/reports/subagents/subagent-master-log.md`.
3. Update this task status and `report:` frontmatter.
4. Move or copy this task to `docs/vault/queue-tasks/review-needed/` or
   `docs/vault/queue-tasks/blocked/`.

## Notes For Codex Review

Confirm the fix is limited to collision response polish and does not become a
physics-system redesign or a movement-feel retune.
