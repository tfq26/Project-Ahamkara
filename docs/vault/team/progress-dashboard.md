# Progress Dashboard

## Snapshot

Overall Status: active
Lead Supervisor: codex-lead-supervisor
Active Workers: 0
Tasks Open: 2
Tasks Claimed: 3
Tasks Needing Review: 0
Tasks Blocked: 5
Tasks Stopped: 0
Tasks Complete: 16

## Active Work

- Blocked: TASK-20260620-1400-level-spec-and-lvl-emitter
- Blocked: TASK-20260620-1415-blender-headless-level-generator
- Blocked: TASK-20260620-1500-textured-material-showcase
- Blocked: TASK-20260620-1510-level-driven-sky-and-fog
- Blocked: TASK-20260620-1520-runtime-confirm-prototype-levels
- Complete: TASK-20260615-1300-ui-screen-split-plan
- Complete: TASK-20260615-1215-render-present-semantics
- Complete: TASK-20260615-1245-input-routing-cleanup
- Complete: TASK-20260622-1100-phase4-reconciliation-replay-fix
- Complete: TASK-20260622-1110-phase4-reliable-channel
- Complete: TASK-20260622-1010-ecs-migration-first-slice
- Complete: TASK-20260620-1415-blender-headless-level-generator
- Complete: TASK-20260622-1020-deterministic-character-controller
- Complete: TASK-20260622-1200-phase4-netcode-milestone-review
- Complete: TASK-20260622-1300-ai-navgrid-astar
- Complete: TASK-20260622-1310-ai-path-follower
- Complete: TASK-20260622-1320-ai-navgrid-from-collision
- Complete: TASK-20260622-1330-milestone-review-ai-nav-and-movement

## Needs User Attention

- Runtime confirmation still needs a machine with a GL display.
- Blender execution still needs a machine with Blender installed and writable
  prefixes.
- Display-backed verification is still needed for the textured material and
  sky/fog showcase tasks.

## Recent Updates

- The UI split-plan analysis slice has been accepted and completed.
- The Phase 4 reconciliation and reliable-channel slices have been accepted and
  completed.
- The Phase 4 milestone review and the AI nav/movement milestone review were
  accepted, and their batched task files were promoted to `completed/`.
- The ECS migration first slice was accepted after revision: the projectile
  migration now preserves the accessor contract and the headless link path is
  guarded.
- The Blender headless level generator was accepted after the Blender 5.1.2
  headless command produced byte-identical `.lvl` output to Path A.
- The Path A, Path B, display-gated showcase, and runtime-confirm tasks are
  blocked by missing runtime/display/tooling capability in this environment.

## Controls

- Stop requests: [[../control/stop-requests]]
- Steering requests: [[../control/steering-requests]]
- Task board: [[agent-task-board]]
