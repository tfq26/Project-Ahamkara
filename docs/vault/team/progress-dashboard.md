# Progress Dashboard

## Snapshot

Overall Status: active
Lead Supervisor: codex-lead-supervisor
Active Workers: 2
Tasks Open: 34
Tasks Claimed: 2
Tasks Needing Review: 4
Tasks Blocked: 7
Tasks Stopped: 0
Tasks Complete: 40

## Active Work

- Open: TASK-20260704-1010-weapon-fire-control
- Open: TASK-20260704-1020-combat-hit-resolution
- Open: TASK-20260704-1030-combat-abilities-core
- Open: TASK-20260704-1100-server-tick-ownership
- Open: TASK-20260704-1110-prediction-reconciliation
- Open: TASK-20260704-1120-snapshot-interpolation-lag-compensation
- Open: TASK-20260704-1130-connection-lifecycle-reliability
- Open: TASK-20260704-1140-network-validation
- Open: TASK-20260704-1200-character-animation-runtime
- Open: TASK-20260704-1210-weapon-animation-layers
- Open: TASK-20260704-1220-audio-subsystem
- Open: TASK-20260704-1230-vfx-feedback
- Open: TASK-20260704-1310-shadows-multi-light
- Open: TASK-20260704-1320-ambient-reflections
- Open: TASK-20260704-1330-ao-anti-aliasing
- Open: TASK-20260704-1340-atmosphere-fog
- Open: TASK-20260704-1350-legacy-gl-retirement
- Open: TASK-20260704-1400-spatial-partitioning
- Open: TASK-20260704-1410-streaming-residency
- Open: TASK-20260704-1420-lod-batching
- Open: TASK-20260704-1430-destination-metadata
- Open: TASK-20260704-1500-inventory-loadout
- Open: TASK-20260704-1510-ai-combatants
- Open: TASK-20260704-1520-encounter-scripting
- Open: TASK-20260704-1530-persistence-rewards
- Open: TASK-20260704-1600-matchmaking-parties
- Open: TASK-20260704-1610-activity-framework
- Open: TASK-20260704-1620-identity-social
- Open: TASK-20260704-1630-live-content-hooks
- Open: TASK-20260704-1700-job-system-frame-allocator
- Open: TASK-20260704-1710-profiling-budgets
- Open: TASK-20260704-1720-authoring-inspector-tooling
- Open: TASK-20260704-1730-telemetry-crash-reporting
- Open: TASK-20260704-1740-ci-packaging
- Claimed: TASK-20260615-1200-client-frame-pipeline
- Claimed: TASK-20260616-1620-ahamkara
- Review Needed: TASK-20260622-1000-fixed-timestep-sim-adoption
- Review Needed: TASK-20260622-1340-ai-nav-agent
- Review Needed: TASK-20260628-0110-collision-response-polish
- Review Needed: TASK-20260704-1000-weapon-runtime-foundation
- Blocked: TASK-20260620-1400-level-spec-and-lvl-emitter
- Blocked: TASK-20260620-1500-textured-material-showcase
- Blocked: TASK-20260620-1510-level-driven-sky-and-fog
- Blocked: TASK-20260620-1520-runtime-confirm-prototype-levels
- Blocked: TASK-20260623-1600-deep-logging-epic
- Blocked: TASK-20260623-1615-deep-logging-wish
- Blocked: TASK-20260623-1616-deep-logging-tools
- Complete: TASK-20260615-1215-render-present-semantics
- Complete: TASK-20260615-1230-pause-menu-state-owner
- Complete: TASK-20260615-1245-input-routing-cleanup
- Complete: TASK-20260615-1300-ui-screen-split-plan
- Complete: TASK-20260620-1200-level-driven-world-meshes
- Complete: TASK-20260620-1330-pbr-uv-plumbing
- Complete: TASK-20260620-1415-blender-headless-level-generator
- Complete: TASK-20260622-1010-ecs-migration-first-slice
- Complete: TASK-20260622-1020-deterministic-character-controller
- Complete: TASK-20260622-1100-phase4-reconciliation-replay-fix
- Complete: TASK-20260622-1110-phase4-reliable-channel
- Complete: TASK-20260622-1200-phase4-netcode-milestone-review
- Complete: TASK-20260622-1300-ai-navgrid-astar
- Complete: TASK-20260622-1310-ai-path-follower
- Complete: TASK-20260622-1320-ai-navgrid-from-collision
- Complete: TASK-20260622-1330-milestone-review-ai-nav-and-movement
- Complete: TASK-20260623-1601-deep-logging-core-foundation
- Complete: TASK-20260623-1602-deep-logging-collision
- Complete: TASK-20260623-1603-deep-logging-physics
- Complete: TASK-20260623-1604-deep-logging-network
- Complete: TASK-20260623-1605-deep-logging-runtime
- Complete: TASK-20260623-1606-deep-logging-platform
- Complete: TASK-20260623-1607-deep-logging-render
- Complete: TASK-20260623-1608-deep-logging-animation
- Complete: TASK-20260623-1609-deep-logging-ui
- Complete: TASK-20260623-1610-deep-logging-input
- Complete: TASK-20260623-1611-deep-logging-audio
- Complete: TASK-20260623-1612-deep-logging-game
- Complete: TASK-20260623-1613-deep-logging-client
- Complete: TASK-20260623-1614-deep-logging-server
- Complete: TASK-20260623-1700-debug-client-frozen-view-investigation
- Complete: TASK-20260628-0101-gameplay-ui-separation
- Complete: TASK-20260628-0102-viewmodel-orientation-contract
- Complete: TASK-20260628-0103-legacy-opengl-compat-collapse
- Complete: TASK-20260628-0104-textured-material-authoring-slice
- Complete: TASK-20260628-0105-level-sky-fog-wiring-slice
- Complete: TASK-20260628-0106-player-movement-camera-controller
- Complete: TASK-20260628-0107-weapon-presentation-separation
- Complete: TASK-20260628-0108-world-orchestration-boundary
- Complete: TASK-20260628-0109-first-person-camera-viewmodel-rig

## Needs User Attention

- Runtime confirmation still needs a machine with a GL display.
- Blender execution still needs a machine with Blender installed and writable
  prefixes.
- Display-backed verification is still needed for the textured material and
  sky/fog showcase tasks.

## Recent Updates

- Five additional deep-logging child tasks were accepted and promoted to
  `completed/`.
- The deep-logging epic was deferred and moved out of open work; the remaining
  deep-logging slices stay blocked until the project is back in a working
  state.
- The frozen-view investigation was accepted after user runtime confirmation
  resolved the last display-backed gap.
- The 2026-06-28 batch of review tasks has been accepted and promoted to
  `completed/`, including the weapon presentation and world orchestration
  revisions.
- The gameplay/runtime ownership roadmap item has been broken into smaller
  queue tasks for movement/camera, weapon presentation, and orchestration
  boundary cleanup.
- The movement/camera controller slice has been completed and promoted to
  `completed/`, and HDR has been removed from the active queue.
- The first-person camera/viewmodel rig slice has been implemented and moved to
  `review-needed/`.
- HDR remains the very last slice in the roadmap; earlier queue items
  explicitly preserve future offscreen-target and post-processing
  compatibility.
- The current roadmap bottleneck is Phase 2 cleanup completion, specifically
  the remaining collision-response polish task, before the sandbox/combat work
  in Phase 3 becomes the next active phase.
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
