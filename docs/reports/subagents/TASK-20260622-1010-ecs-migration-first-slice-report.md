---
type: subagent-report
category: verification
status: validated_with_known_gaps
created: 2026-06-22
agent: opencode
subsystems:
  - game
branch: main (on checkpoint 43ba9cd)
validation:
  - "code inspection (verify-first); no code changed"
---

# Subagent Report

## Task

ECS migration first slice: move one authoritative entity type fully onto
`entt::registry`, removing its fixed array (roadmap Phase 0 / Part III Stream 3).
Task: `TASK-20260622-1010-ecs-migration-first-slice`.

## Status

validated_with_known_gaps — **substantially already implemented**; the literal
"remove the fixed array" acceptance criterion is NOT met and is non-trivial.

## Evidence

- Projectiles + dummies are already **simulated via `entt::registry`**:
  `game/src/world_projectile.cpp` (`registry.view<WorldProjectileComponent>()`,
  `registry.create/emplace/destroy`), `game/src/world_dummy_sim.cpp` (free system
  functions over `registry.view<TargetDummyComponent>()`), `world.cpp` emplaces
  the components. `game/include/.../components.h` defines Transform/Health/
  Projectile/Lifetime/Movement components.
- BUT `world.h` still keeps `ProjectileState projectiles_[kMaxProjectiles]` and
  `TargetDummyState dummies_[kMaxDummies]`, kept in sync from the registry
  (`world.cpp:754` view→array; `:769` has a "Future: populate projectiles_[]
  from EnTT" comment).

## Why The Remaining Step Isn't A Clean Slice

The public accessors return contiguous pointers (`get_dummies() -> const
TargetDummyState*`, `get_projectiles() -> ProjectileState*`) consumed by
networking (`net_types.h` `ProjectileState[8]`/`TargetDummyState[4]`), snapshots,
history, and the Jolt bridge. Removing the arrays while keeping those pointer
accessors ripples across net/render/snapshot code — beyond a behavior-preserving
"first slice."

## Recommendation

Close as substantially-done, OR re-scope to a non-rippling step (e.g. make the
registry the sole store and change accessors to a span/iterator API in a
dedicated, reviewed pass — not a "first slice"). No code changed here.

## Confidence

high — registry-driven sim and the residual synced arrays are both directly
evidenced.
