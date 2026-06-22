---
type: opencode-task
status: review-needed
created: 2026-06-22
queued_by: codex
assigned_to: opencode
priority: normal
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - game
related_feature:
report: ../../../reports/subagents/TASK-20260622-1010-ecs-migration-first-slice-report.md
---

# TASK-20260622-1010-ecs-migration-first-slice

# Goal

Migrate ONE authoritative entity type fully onto `entt::registry` (components +
a system free function), removing its fixed-size array while keeping the public
accessors stable. Roadmap **Phase 0 / Simulation Data Model** (Part III Stream 3).

## Roadmap Source

`docs/roadmap/roadmap.md` — Part I Phase 0 §"Begin ECS adoption", Part II §3,
Part III Stream 3.

## Verify First (the tree has in-progress work)

EnTT is a dependency and `World` already holds a registry; some migration may be
partial. Read `game/src/world.cpp`, `game/src/world_projectile.{h,cpp}`,
`game/src/world_dummy_sim.{h,cpp}`, `game/include/ahamkara/game/world.h`, and any
`components.h`. Pick the entity type that is LEAST migrated and finish it.

## Scope

In bounds:
- Define the component struct(s) for the chosen type (e.g. Transform/Lifetime/
  Projectile or Health/Dummy).
- Replace its fixed-size array with `registry` storage; convert its update to a
  free `system` function iterating `registry.view<...>()`.
- Keep existing public accessors (e.g. `get_*_state`) working, backed by the
  registry.
- No `std::unordered_map` for ordered sim state (determinism).

Out of bounds:
- Migrating every entity type; ECS-driven networking/replication.

## Acceptance Bar

- The chosen type has no fixed-size entity array; its update iterates a registry
  view; public accessors unchanged.
- `ahamkara_world_tests` (and others) pass without modification.
- Build + existing tests stay green.

## Validation

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

## Reporting Required

Standard: report, master log, update task `report:`/status, move to
`review-needed/` or `blocked/`.
