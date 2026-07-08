# Supervisor Decisions

Append one decision block for each reviewed task.

## 2026-06-22 - TASK-20260620-1400-level-spec-and-lvl-emitter - verify

Supervisor: codex-lead-supervisor
Worker: opencode
Decision: verify
Evidence Checked:
- `docs/reports/subagents/TASK-20260620-1400-level-spec-and-lvl-emitter-report.md`
- `docs/reports/subagents/TASK-20260620-1400-level-spec-and-lvl-emitter-codex-review.md`
- `docs/reports/subagents/subagent-master-log.md`
- `docs/vault/queue-tasks/review-needed/TASK-20260620-1400-level-spec-and-lvl-emitter.md`
- `git status --short`
Reason:
- The Path A emitter, generated assets, importer output, and tests are in
  place.
- The task still depends on a visible runtime confirmation of the prototype
  mesh in the client.
Next Actions:
- Keep the task in `review-needed/` until the runtime check is completed.
Completion Bar:
- Prototype level is visually confirmed in the client.
- The report and evidence remain aligned.
User Visible Notes:
- Do not claim completion yet; runtime confirmation is still missing.

## 2026-06-22 - TASK-20260620-1415-blender-headless-level-generator - verify

Supervisor: codex-lead-supervisor
Worker: opencode
Decision: verify
Evidence Checked:
- `docs/reports/subagents/TASK-20260620-1415-blender-headless-level-generator-report.md`
- `docs/reports/subagents/TASK-20260620-1415-blender-headless-level-generator-codex-review.md`
- `docs/reports/subagents/TASK-20260620-1415-blender-headless-level-generator.md`
- `docs/reports/subagents/TASK-20260620-1400-level-spec-and-lvl-emitter-report.md`
- `docs/reports/subagents/subagent-master-log.md`
- `git status --short`
Reason:
- The shared `.lvl` writer and bpy-free testable layer are implemented.
- The Blender-dependent export path has not been executed in this environment.
Next Actions:
- Keep the task in `review-needed/` until the Blender run proof is available.
Completion Bar:
- The documented `blender -b -P` command runs successfully on a machine with
  Blender installed.
- The `.blend` and glTF outputs are confirmed.
User Visible Notes:
- Do not claim completion yet; the Blender execution path is still unverified.

## 2026-06-22 - TASK-20260615-1300-ui-screen-split-plan - complete

Supervisor: codex-lead-supervisor
Worker: opencode
Decision: complete
Evidence Checked:
- `docs/reports/subagents/TASK-20260615-1300-ui-screen-split-plan-report.md`
- `docs/reports/subagents/TASK-20260615-1300-ui-screen-split-plan-codex-review.md`
- `docs/vault/queue-tasks/completed/TASK-20260615-1300-ui-screen-split-plan.md`
- `docs/reports/subagents/subagent-master-log.md`
Reason:
- The report correctly mapped the current UI layout and identified the first
  safe extraction slice.
- No code change was required for this prep slice.
Next Actions:
- Move on to the helper-extraction follow-up slice.
Completion Bar:
- UI prep slice documented with a concrete next step.
User Visible Notes:
- This slice is accepted and complete.

## 2026-06-22 - TASK-20260620-1400-level-spec-and-lvl-emitter - blocked

Supervisor: codex-lead-supervisor
Worker: opencode
Decision: blocked
Evidence Checked:
- `docs/reports/subagents/TASK-20260620-1400-level-spec-and-lvl-emitter-report.md`
- `docs/reports/subagents/TASK-20260620-1400-level-spec-and-lvl-emitter-codex-review-final.md`
- `docs/vault/queue-tasks/blocked/TASK-20260620-1400-level-spec-and-lvl-emitter.md`
- `echo $DISPLAY`
- `which Xvfb`
- `which blender`
Reason:
- The Path A authoring work is implemented and validated, but the acceptance
  bar still depends on runtime confirmation in a GL window.
- The current environment cannot provide that confirmation.
Next Actions:
- Re-run the runtime confirmation on a machine with a display.
Completion Bar:
- Prototype level visually confirmed in the client.
User Visible Notes:
- Keep blocked until the display-backed runtime check exists.

## 2026-06-22 - TASK-20260620-1415-blender-headless-level-generator - blocked

Supervisor: codex-lead-supervisor
Worker: opencode
Decision: blocked
Evidence Checked:
- `docs/reports/subagents/TASK-20260620-1415-blender-headless-level-generator-report.md`
- `docs/reports/subagents/TASK-20260620-1415-blender-headless-level-generator-codex-review-final.md`
- `docs/vault/queue-tasks/blocked/TASK-20260620-1415-blender-headless-level-generator.md`
- `brew info blender`
- `brew install --cask blender`
Reason:
- The pure logic and shared `.lvl` writer are implemented, but the documented
  Blender execution path cannot be run here.
- Blender is not installed, and the local Homebrew prefix is not writable.
Next Actions:
- Re-run the headless Blender command on a machine where Blender can be
  installed and executed.
Completion Bar:
- The documented Blender command runs successfully.
User Visible Notes:
- Keep blocked until Blender can be executed.

## 2026-06-22 - TASK-20260620-1500-textured-material-showcase - blocked

Supervisor: codex-lead-supervisor
Worker: opencode
Decision: blocked
Evidence Checked:
- `docs/reports/subagents/TASK-20260620-1500-textured-material-showcase-report.md`
- `docs/vault/queue-tasks/blocked/TASK-20260620-1500-textured-material-showcase.md`
- `tools/levelgen/gen_textured_cube.py`
- `assets/manifest.assets`
Reason:
- The authored cube, material, and importer path are implemented and build-test
  clean.
- The acceptance bar still requires visible runtime confirmation of texture
  sampling, which this environment cannot provide.
Next Actions:
- Re-run the showcase on a display-capable machine and confirm the texture is
  visibly sampling on the mesh.
Completion Bar:
- The textured showcase is visually confirmed in the client.
User Visible Notes:
- Keep blocked until the display-backed verification exists.

## 2026-06-22 - TASK-20260620-1510-level-driven-sky-and-fog - blocked

Supervisor: codex-lead-supervisor
Worker: opencode
Decision: blocked
Evidence Checked:
- `docs/reports/subagents/TASK-20260620-1510-level-driven-sky-and-fog-report.md`
- `docs/vault/queue-tasks/blocked/TASK-20260620-1510-level-driven-sky-and-fog.md`
- `engine/render/src/debug_renderer.cpp`
- `client/src/debug_client.cpp`
Reason:
- The level environment override is implemented and build-validated.
- The acceptance bar still requires a visible GL-backed check of the sky/fog
  change, which this environment cannot provide.
Next Actions:
- Re-run the level on a display-capable machine and confirm the environment
  override is visible.
Completion Bar:
- The sky/fog override is visually confirmed in the client.
User Visible Notes:
- Keep blocked until the display-backed verification exists.

## 2026-06-23 - TASK-20260622-1010-ecs-migration-first-slice - complete

Supervisor: codex-lead-supervisor
Worker: opencode
Decision: complete
Evidence Checked:
- `docs/reports/subagents/TASK-20260622-1010-ecs-migration-first-slice-report.md`
- `docs/reports/subagents/TASK-20260622-1010-ecs-migration-first-slice-codex-review.md`
- `game/include/ahamkara/game/world.h`
- `game/src/world.cpp`
- `game/CMakeLists.txt`
- `tests/CMakeLists.txt`
- `client/include/ahamkara/client/local_play.h`
- `client/src/local_play.cpp`
- `client/src/threaded_local_runtime.cpp`
- `game/src/activities/deathmatch_activity.cpp`
Reason:
- The projectile slice now preserves the original accessor contract while using
  the registry as the authoritative store.
- The fixed-size projectile array is removed and the headless render link gap
  is guarded, so the documented validation now passes.
Next Actions:
- None.
Completion Bar:
- Projectile ECS migration accepted and closed.
User Visible Notes:
- Accepted after the second pass fixed the accessor contract and headless
  validation path.

## 2026-06-23 - TASK-20260620-1415-blender-headless-level-generator - complete

Supervisor: codex-lead-supervisor
Worker: opencode
Decision: complete
Evidence Checked:
- `docs/reports/subagents/TASK-20260620-1415-blender-headless-level-generator-report.md`
- `docs/reports/subagents/TASK-20260620-1415-blender-headless-level-generator-codex-review.md`
- `tools/blender/build_level.py`
- `tools/blender/test_build_level.py`
- `docs/vault/queue-tasks/completed/TASK-20260620-1415-blender-headless-level-generator.md`
Reason:
- The shared `.lvl` writer is reused, the bpy-free layer is testable, and the
  Blender headless command was actually executed and validated.
- The produced `.lvl` matches Path A byte-for-byte for both prototype specs.
Next Actions:
- None.
Completion Bar:
- Headless Blender generator accepted and closed.
User Visible Notes:
- Accepted after the Blender execution proof was produced.

## 2026-06-22 - TASK-20260620-1520-runtime-confirm-prototype-levels - blocked

Supervisor: codex-lead-supervisor
Worker: codex
Decision: blocked
Evidence Checked:
- `docs/reports/subagents/TASK-20260620-1520-runtime-confirm-prototype-levels-report.md`
- `docs/vault/queue-tasks/blocked/TASK-20260620-1520-runtime-confirm-prototype-levels.md`
- `echo $DISPLAY`
- `which Xvfb`
Reason:
- The runtime-confirm task requires a GL display.
- This environment has neither a display nor a windowing fallback.
Next Actions:
- Run the confirmation on a display-capable machine.
Completion Bar:
- A written runtime confirmation or screenshot exists.
User Visible Notes:
- The missing display is the only blocker.

## 2026-06-22 - TASK-20260615-1215-render-present-semantics - complete

Supervisor: codex-lead-supervisor
Worker: opencode
Decision: complete
Evidence Checked:
- `docs/reports/subagents/TASK-20260615-1215-render-present-semantics-report.md`
- `docs/reports/subagents/TASK-20260615-1215-render-present-semantics-codex-review.md`
- `docs/vault/queue-tasks/completed/TASK-20260615-1215-render-present-semantics.md`
- `engine/render/include/ae/render/debug_renderer.h`
- `engine/render/src/debug_renderer.cpp`
- `engine/render/include/ae/render/render_backend.h`
Reason:
- The task’s contract clarification is clear and scoped.
- The change is behavior-stable and validated by build plus tests.
Next Actions:
- None.
Completion Bar:
- Render and present semantics are explicit in the API docs.
User Visible Notes:
- Accepted as complete.

## 2026-06-22 - TASK-20260615-1245-input-routing-cleanup - complete

Supervisor: codex-lead-supervisor
Worker: opencode
Decision: complete
Evidence Checked:
- `docs/reports/subagents/TASK-20260615-1245-input-routing-cleanup-report.md`
- `docs/reports/subagents/TASK-20260615-1245-input-routing-cleanup-codex-review.md`
- `docs/vault/queue-tasks/completed/TASK-20260615-1245-input-routing-cleanup.md`
- `client/src/client_frame_pipeline.cpp`
- `engine/platform/src/window_glfw.cpp`
Reason:
- The duplicated raw GLFW ESC path was removed and routing now uses the
  platform edge-trigger.
- Validation supports the localized behavior-preserving cleanup.
Next Actions:
- None.
Completion Bar:
- The cleanup is localized, behavior-preserving, and validated.
User Visible Notes:
- Accepted as complete.

## 2026-06-22 - TASK-20260622-1100-phase4-reconciliation-replay-fix - complete

Supervisor: codex-lead-supervisor
Worker: opencode
Decision: complete
Evidence Checked:
- `docs/reports/subagents/TASK-20260622-1100-phase4-reconciliation-replay-fix-report.md`
- `docs/reports/subagents/TASK-20260622-1100-phase4-reconciliation-replay-fix-codex-review.md`
- `docs/vault/queue-tasks/completed/TASK-20260622-1100-phase4-reconciliation-replay-fix.md`
- `game/src/client_prediction.cpp`
- `tests/src/world_tests.cpp`
Reason:
- The first-snapshot replay guard has been removed.
- The deterministic regression test proves the fix.
Next Actions:
- None.
Completion Bar:
- First-snapshot reconciliation replays unacked inputs.
User Visible Notes:
- Accepted as complete.

## 2026-06-22 - TASK-20260622-1110-phase4-reliable-channel - complete

Supervisor: codex-lead-supervisor
Worker: opencode
Decision: complete
Evidence Checked:
- `docs/reports/subagents/TASK-20260622-1110-phase4-reliable-channel-report.md`
- `docs/reports/subagents/TASK-20260622-1110-phase4-reliable-channel-codex-review.md`
- `docs/vault/queue-tasks/completed/TASK-20260622-1110-phase4-reliable-channel.md`
- `engine/network/include/ae/network/reliable_channel.h`
- `tests/src/reliable_channel_tests.cpp`
Reason:
- The transport-agnostic reliable channel satisfies the requested ACK and
  retransmit behavior.
- Unit tests cover ACK removal, timeout retransmits, and wraparound.
Next Actions:
- None.
Completion Bar:
- Reliable channel implemented and unit-tested.
User Visible Notes:
- Accepted as complete.

## 2026-06-22 - TASK-20260622-1010-ecs-migration-first-slice - revise

Supervisor: codex-lead-supervisor
Worker: opencode
Decision: revise
Evidence Checked:
- `docs/reports/subagents/TASK-20260622-1010-ecs-migration-first-slice-report.md`
- `docs/reports/subagents/TASK-20260622-1010-ecs-migration-first-slice-codex-review.md`
- `game/include/ahamkara/game/world.h`
- `game/src/world.cpp`
- `game/src/world_projectile.cpp`
- `game/src/world_dummy_sim.cpp`
Reason:
- The fixed arrays are still present and still back the public accessors.
- The acceptance bar explicitly requires removing the fixed array.
Next Actions:
- Implement the array-removal/accessor rewrite or rescope the migration into a
  smaller reviewed slice.
Completion Bar:
- One entity type is registry-only and the fixed array is gone.
User Visible Notes:
- Needs a real implementation slice, not just verification.

## 2026-06-22 - TASK-20260622-1020-deterministic-character-controller - revise

Supervisor: codex-lead-supervisor
Worker: opencode
Decision: revise
Evidence Checked:
- `docs/reports/subagents/TASK-20260622-1020-deterministic-character-controller-report.md`
- `docs/reports/subagents/TASK-20260622-1020-deterministic-character-controller-codex-review.md`
- `game/src/movement.cpp`
- `game/include/ahamkara/game/movement.h`
- `game/src/game_module.cpp`
- `tests/src/movement_tests.cpp`
Reason:
- The movement path is deterministic, but the tunable `ConfigVar`s are not wired
  into the active `MovementConfig`.
- That means the hot-reloadable tuning requirement is still unmet.
Next Actions:
- Wire the config vars into movement and extend tests for the live tuning path.
Completion Bar:
- Movement tuning is driven by the registered `ConfigVar`s.
User Visible Notes:
- Needs the config wiring slice before completion.

## 2026-06-22 - TASK-20260622-1200-phase4-netcode-milestone-review - complete

Supervisor: codex-lead-supervisor
Worker: opencode
Decision: complete
Evidence Checked:
- `docs/reports/subagents/TASK-20260622-1200-phase4-netcode-milestone-review-report.md`
- `docs/reports/subagents/TASK-20260622-1100-phase4-reconciliation-replay-fix-report.md`
- `docs/reports/subagents/TASK-20260622-1110-phase4-reliable-channel-report.md`
- `game/src/client_prediction.cpp`
- `engine/network/include/ae/network/reliable_channel.h`
Reason:
- The reviewed Phase 4 slices are complete and regression-tested.
- The remaining live-loop socket integration is correctly separated as a follow-up.
Next Actions:
- Track the socket-capable live-loop wiring as a separate task.
Completion Bar:
- Phase 4 milestone review accepted and closed.
User Visible Notes:
- The milestone is complete; the socket integration follow-up remains open.

## 2026-06-22 - TASK-20260622-1020-deterministic-character-controller - complete

Supervisor: codex-lead-supervisor
Worker: opencode
Decision: complete
Evidence Checked:
- `docs/reports/subagents/TASK-20260622-1020-deterministic-character-controller-codex-review.md`
- `docs/reports/subagents/TASK-20260622-1020-deterministic-character-controller-impl-report.md`
- `game/include/ahamkara/game/game_module.h`
- `game/src/game_module.cpp`
- `game/src/world.cpp`
- `tests/src/world_tests.cpp`
Reason:
- The movement config vars are wired into the runtime movement path.
- The defaults preserve the previous feel while making tuning hot-reloadable.
Next Actions:
- None.
Completion Bar:
- Movement tuning is driven by the registered `game.player_*` ConfigVars.
User Visible Notes:
- Accepted as complete after the config wiring landed.

## 2026-06-22 - TASK-20260622-1300-ai-navgrid-astar - complete

Supervisor: codex-lead-supervisor
Worker: opencode
Decision: complete
Evidence Checked:
- `docs/reports/subagents/TASK-20260622-1300-ai-navgrid-astar-report.md`
- `docs/reports/subagents/TASK-20260622-1330-milestone-review-ai-nav-and-movement-report.md`
- `game/include/ahamkara/game/ai/nav_grid.h`
- `tests/src/nav_grid_tests.cpp`
Reason:
- The deterministic `NavGrid` and A* foundation is implemented and covered by tests.
- The milestone review accepted the batch as complete.
Next Actions:
- None.
Completion Bar:
- AI nav grid and pathfinding foundation implemented and validated.
User Visible Notes:
- Accepted as part of the AI nav/movement milestone.

## 2026-06-22 - TASK-20260622-1310-ai-path-follower - complete

Supervisor: codex-lead-supervisor
Worker: opencode
Decision: complete
Evidence Checked:
- `docs/reports/subagents/TASK-20260622-1310-ai-path-follower-report.md`
- `docs/reports/subagents/TASK-20260622-1330-milestone-review-ai-nav-and-movement-report.md`
- `game/include/ahamkara/game/ai/path_follower.h`
- `tests/src/nav_grid_tests.cpp`
Reason:
- The path follower and waypoint conversion are implemented and unit-tested.
- The milestone review accepted the batch as complete.
Next Actions:
- None.
Completion Bar:
- Path follower slice implemented and validated.
User Visible Notes:
- Accepted as part of the AI nav/movement milestone.

## 2026-06-22 - TASK-20260622-1320-ai-navgrid-from-collision - complete

Supervisor: codex-lead-supervisor
Worker: opencode
Decision: complete
Evidence Checked:
- `docs/reports/subagents/TASK-20260622-1320-ai-navgrid-from-collision-report.md`
- `docs/reports/subagents/TASK-20260622-1330-milestone-review-ai-nav-and-movement-report.md`
- `game/include/ahamkara/game/ai/nav_grid.h`
- `tests/src/nav_grid_tests.cpp`
Reason:
- Collision-to-grid rasterization is implemented and tested.
- The milestone review accepted the batch as complete.
Next Actions:
- None.
Completion Bar:
- Nav-grid-from-collision slice implemented and validated.
User Visible Notes:
- Accepted as part of the AI nav/movement milestone.

## 2026-06-22 - TASK-20260622-1330-milestone-review-ai-nav-and-movement - complete

Supervisor: codex-lead-supervisor
Worker: opencode
Decision: complete
Evidence Checked:
- `docs/reports/subagents/TASK-20260622-1330-milestone-review-ai-nav-and-movement-report.md`
- `docs/reports/subagents/TASK-20260622-1020-deterministic-character-controller-impl-report.md`
- `docs/reports/subagents/TASK-20260622-1300-ai-navgrid-astar-report.md`
- `docs/reports/subagents/TASK-20260622-1310-ai-path-follower-report.md`
- `docs/reports/subagents/TASK-20260622-1320-ai-navgrid-from-collision-report.md`
Reason:
- The movement-config wiring and AI navigation foundation are complete.
- The review correctly accepted the full headless batch.
Next Actions:
- None.
Completion Bar:
- AI nav/movement milestone review accepted and closed.
User Visible Notes:
- The batch is complete and promoted to `completed/`.
