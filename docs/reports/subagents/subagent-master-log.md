# Subagent Master Log

This log indexes Ahamkara-specific subagent reports. Full reports live in this
folder as dated Markdown files.

Do not add reports for other projects here.
## TASK-20260615-1230-pause-menu-state-owner — Completed 2026-06-15
Centralized pause/menu state ownership in ClientMenuState. Report: reports/subagents/TASK-20260615-1230-pause-menu-state-owner-report.md
## TASK-20260615-1230-pause-menu-state-owner — Revision 2 2026-06-15
Removed duplicate screen_ member. menu_state_ is now the sole source of truth — ahamkara_ui.cpp mutations are immediately visible. Removed sync_screen().
## TASK-20260615-1200-client-frame-pipeline — Completed 2026-06-15
Extracted ClientFramePipeline with 10 named frame stages. debug_client.cpp down from 253 to 117 lines. Report: reports/subagents/TASK-20260615-1200-client-frame-pipeline-report.md
## TASK-20260615-1200-client-frame-pipeline — Re-queued 2026-06-15
Verified build passes. Report frontmatter fixed. Moved back to review-needed.
## TASK-20260620-1200-level-driven-world-meshes — Review needed 2026-06-20
Activated the dead PbrRenderer path: levels now drive scalar-PBR world meshes via a new LevelRenderScene + DebugRenderer camera getters, wired through render_local_debug_frame. Build (debug) + 10/10 tests pass (new ahamkara_level_render_tests). Not runtime-confirmed; textures inert (no UVs); no authored level has mesh instances yet. Report: reports/subagents/TASK-20260620-1200-level-driven-world-meshes-report.md
## TASK-20260620-1330-pbr-uv-plumbing — Review needed 2026-06-20
Plumbed UVs end to end (glTF TEXCOORD_0 -> .aemesh v2 + v1 back-compat -> GpuMesh.vbo_texcoords -> PBR aTexCoord/vUV). PBR now samples albedo/ORM at real UVs instead of vec2(0). Build (debug) + 10/10 tests pass (new UV roundtrip). Not runtime-confirmed; tangents/normal-map deferred. Report: reports/subagents/TASK-20260620-1330-pbr-uv-plumbing-report.md
## TASK-20260620-1200-level-driven-world-meshes — Revise 2026-06-20
Codex review found the PBR level-mesh pass runs after DebugRenderer screen-space overlays, so authored meshes can overwrite HUD/crosshair/menu overlays. Also missing requested GL-free level-to-draw-call assembly coverage. Review: reports/subagents/TASK-20260620-1200-level-driven-world-meshes-codex-review.md
## TASK-20260620-1330-pbr-uv-plumbing — Completed 2026-06-20
Codex accepted the scoped UV plumbing. Build debug and 10/10 tests pass locally; runtime textured output remains a follow-up once authored content exists. Review: reports/subagents/TASK-20260620-1330-pbr-uv-plumbing-codex-review.md
## TASK-20260620-1200-level-driven-world-meshes — Revised, review needed 2026-06-20
Addressed Codex review: PBR level-mesh pass now runs via a DebugRenderer world-phase callback (before screen-space overlays) so it cannot overwrite HUD/crosshair/menu; added GL-free make_level_draw_call helper + test_draw_call_assembly (field assembly + per-mesh draw-call count). Build (debug) clean; 10/10 tests pass. Still not runtime-confirmed. Report: reports/subagents/TASK-20260620-1200-level-driven-world-meshes-report.md
## TASK-20260620-1400-level-spec-and-lvl-emitter — Review needed 2026-06-20
Added Path A authoring: JSON level spec + tools/levelgen/spec_to_lvl.py emitter (+ --selftest), two prototype levels (arena + test_box mesh showcase), manifest entries; importer compiled all 4 assets (0 failed). Build (debug) + 10/10 tests pass; emitter selftest green. Not runtime-confirmed in a GL window (load assets/compiled/levels/prototype_box.aelevel to verify). Report: reports/subagents/TASK-20260620-1400-level-spec-and-lvl-emitter-report.md
## TASK-20260620-1415-blender-headless-level-generator — Review needed 2026-06-20
Added Path B: tools/blender/build_level.py (bpy-guarded) reusing the Path A .lvl writer, plus a bpy-free unit test. Unit test + .lvl parity with Path A pass; build + 10/10 tests green. Blender run not executed (no Blender installed); glTF/.blend path authored but unverified. Report: reports/subagents/TASK-20260620-1415-blender-headless-level-generator-report.md
## TASK-20260620-1200-level-driven-world-meshes — Completed 2026-06-22
Re-reviewed after revision: the PBR level-mesh pass now runs before overlays and the GL-free draw-call assembly test is in place. Report: reports/subagents/TASK-20260620-1200-level-driven-world-meshes-report.md
## TASK-20260620-1400-level-spec-and-lvl-emitter — Review needed 2026-06-22
Path A authoring is implemented and green, but runtime-visible mesh confirmation is still missing. Review: reports/subagents/TASK-20260620-1400-level-spec-and-lvl-emitter-codex-review.md
## TASK-20260620-1415-blender-headless-level-generator — Review needed 2026-06-22
Path B authoring is implemented and unit-tested, but the Blender-dependent export run is still unverified. Review: reports/subagents/TASK-20260620-1415-blender-headless-level-generator-codex-review.md
## TASK-20260620-1400-level-spec-and-lvl-emitter — Re-reviewed 2026-06-22
Kept at `verify`: the shared JSON spec/emitter path is in scope and the build plus task-specific tests are green, but the prototype level still lacks manual runtime confirmation. The full `run-tests.sh` command trips sandbox-specific socket permission failures in unrelated network tests.
## TASK-20260620-1415-blender-headless-level-generator — Re-reviewed 2026-06-22
Kept at `verify`: the bpy-free layer and shared `.lvl` writer are in scope and build/testable, but Blender has not been executed here. The full `run-tests.sh` command trips sandbox-specific socket permission failures in unrelated network tests.
## TASK-20260615-1300-ui-screen-split-plan — Review needed 2026-06-22
Analysis slice (no code change). Mapped UI layout: state is cleanly owned, but render is split across ahamkara_ui.cpp (engine screens + anon-namespace theme/widgets) and debug_ui_controller.cpp (scoreboard/death/matchend + DUPLICATED theme/widgets). Blocker: a screen can't move to its own TU until the anon-namespace helpers are extracted. Recommended slice 1 = extract a shared ae::ui::detail widgets header (collision-free, behavior-preserving), then per-screen TU split, then render/action-intent boundary. Report: reports/subagents/TASK-20260615-1300-ui-screen-split-plan-report.md
## TASK-20260615-1300-ui-screen-split-plan — Completed 2026-06-22
Analysis slice accepted: the report correctly maps the UI layout, identifies the helper-coupling blocker, and leaves a concrete safe first extraction slice. Review: reports/subagents/TASK-20260615-1300-ui-screen-split-plan-codex-review.md
## TASK-20260620-1400-level-spec-and-lvl-emitter — Blocked 2026-06-22
Path A authoring is implemented and green, but the runtime-visible mesh confirmation cannot be completed here because there is no GL display / windowing stack. Review: reports/subagents/TASK-20260620-1400-level-spec-and-lvl-emitter-codex-review-final.md
## TASK-20260620-1415-blender-headless-level-generator — Blocked 2026-06-22
Path B authoring is implemented and unit-tested, but Blender is unavailable and the headless export proof cannot be produced in this environment. Review: reports/subagents/TASK-20260620-1415-blender-headless-level-generator-codex-review-final.md
## TASK-20260620-1520-runtime-confirm-prototype-levels — Blocked 2026-06-22
Runtime confirmation cannot be completed here because the environment has no GL display, no Xvfb, and no windowing fallback. Report: reports/subagents/TASK-20260620-1520-runtime-confirm-prototype-levels-report.md
## TASK-20260615-1245-input-routing-cleanup — Review needed 2026-06-22
Removed the duplicated/bypassed ESC menu-toggle path in ClientFramePipeline::stage_handle_menu_and_hotkeys (raw glfwGetKey + process-static esc_was_down); menu toggle now uses the single platform is_key_pressed(Escape) edge-trigger + controller binding. Verified is_key_pressed is genuinely edge-triggered in window_glfw.cpp. Build (debug) + 10/10 tests pass; not runtime-confirmed (no display). Overlaps the claimed client-frame-pipeline task. Report: reports/subagents/TASK-20260615-1245-input-routing-cleanup-report.md
## TASK-20260615-1215-render-present-semantics — Review needed 2026-06-22
Clarified render-vs-present contract (docstrings on DebugRenderer render/present/set_auto_present + inline comment on the auto_present path + tightened RenderBackend::end_frame doc). Documentation-only, behavior-stable; auto_present kept for non-staged callers. Build (debug) + 10/10 tests pass. Report: reports/subagents/TASK-20260615-1215-render-present-semantics-report.md
## TASK-20260620-1510-level-driven-sky-and-fog — Build-validated, blocked on display 2026-06-22
Implemented level-environment override: DebugRenderer::set_level_environment drives clear/sky/fog color from level sky_color and ambient from level ambient; debug_client sets it at level load; day/night fallback preserved. Build (debug) + 10/10 tests pass. Routed to blocked/ because the visible sky/ambient change needs a GL display (per user: build-validatable parts done now). Report: reports/subagents/TASK-20260620-1510-level-driven-sky-and-fog-report.md
## TASK-20260620-1500-textured-material-showcase — Build-validated, blocked on display 2026-06-22
Authored a UV-mapped textured cube (new tools/levelgen/gen_textured_cube.py: glTF+bin with TEXCOORD_0 + uint32 indices, 32-bit TGA, .mat) + a textured_showcase level; manifest wired. Importer: Imported 4, failed 0. Build + 10/10 tests pass. Routed to blocked/ because the texture visibly sampling on the mesh needs a GL display. Report: reports/subagents/TASK-20260620-1500-textured-material-showcase-report.md
## TASK-20260615-1215-render-present-semantics — Completed 2026-06-22
Accepted: the report and diff clarify render vs present semantics without behavior change. Review: reports/subagents/TASK-20260615-1215-render-present-semantics-codex-review.md
## TASK-20260615-1245-input-routing-cleanup — Completed 2026-06-22
Accepted: the raw GLFW ESC path was removed, the single platform edge-trigger remains, and build + tests are green. Review: reports/subagents/TASK-20260615-1245-input-routing-cleanup-codex-review.md
## TASK-20260622-1100-phase4-reconciliation-replay-fix — Completed 2026-06-22
Accepted: the first-snapshot replay guard is removed and the deterministic regression test covers the fix. Review: reports/subagents/TASK-20260622-1100-phase4-reconciliation-replay-fix-codex-review.md
## TASK-20260622-1110-phase4-reliable-channel — Completed 2026-06-22
Accepted: the header-only reliable channel and unit tests satisfy the transport-agnostic reliability slice. Review: reports/subagents/TASK-20260622-1110-phase4-reliable-channel-codex-review.md
## TASK-20260622-1020-deterministic-character-controller — Self-validated (batched for milestone review) 2026-06-22
Implemented the residual: wired the game.player_* ConfigVars into world.cpp's runtime movement (walk/sprint/jump/gravity) via game_module accessors; aligned ConfigVar defaults to the prior constants (behavior-preserving) so tuning is now hot-reloadable. Added test_movement_config_wiring. Build (debug) + 11/11 tests pass. Per the current workflow this is self-validated, not sent to Codex individually. Report: reports/subagents/TASK-20260622-1020-deterministic-character-controller-impl-report.md
## TASK-20260622-1300-ai-navgrid-astar — Self-validated (batched for milestone review) 2026-06-22
Phase 8 AI start: header-only NavGrid + deterministic A* (game/include/ahamkara/game/ai/nav_grid.h) with 4/8-connectivity, no corner-cutting, stable tie-break. New ahamkara_nav_grid_tests (7 cases). Build (debug) + 12/12 tests pass. Navigation math only; not yet wired into World. Self-validated, batched. Report: reports/subagents/TASK-20260622-1300-ai-navgrid-astar-report.md
## TASK-20260622-1310-ai-path-follower — Self-validated (batched for milestone review) 2026-06-22
Phase 8 AI: header-only PathFollower + grid_path_to_waypoints (game/include/ahamkara/game/ai/path_follower.h); steers a point along grid-derived waypoints at fixed speed, deterministic. Tests folded into nav_grid_tests (3 cases). Build (debug) + 12/12 tests pass. Self-validated, batched. Report: reports/subagents/TASK-20260622-1310-ai-path-follower-report.md
## TASK-20260622-1320-ai-navgrid-from-collision — Self-validated (batched for milestone review) 2026-06-22
Phase 8 AI: added NavAABB + build_nav_grid (rasterize world-space collision rects into a NavGrid; decoupled from LevelAsset) to ai/nav_grid.h. Tests folded into nav_grid_tests (2 cases). Build (debug) + 12/12 tests pass. Completes the AI nav foundation (grid/A* + path follower + build-from-collision). Self-validated, batched. Report: reports/subagents/TASK-20260622-1320-ai-navgrid-from-collision-report.md
## TASK-20260622-1340-ai-nav-agent — Self-validated (batched for milestone review) 2026-06-22
Phase 8 AI: NavSpace (world↔cell) + NavAgent (plan A* path to a world goal, follow on fixed dt) in ai/nav_agent.h. End-to-end tests in nav_grid_tests: collision rects → grid → A* → follow → reach goal; no-path case. Build (debug) + 12/12 tests pass. Self-validated, batched (added to TASK-20260622-1330). Report: reports/subagents/TASK-20260622-1340-ai-nav-agent-report.md
## TASK-20260622-1010-ecs-migration-first-slice — Revise 2026-06-22
The report is honest about the residual arrays: the task's fixed-array removal acceptance bar is not yet met. Review: reports/subagents/TASK-20260622-1010-ecs-migration-first-slice-codex-review.md
## TASK-20260622-1020-deterministic-character-controller — Revise 2026-06-22
Movement is already deterministic, but the ConfigVar wiring required by the task is still missing. Review: reports/subagents/TASK-20260622-1020-deterministic-character-controller-codex-review.md
## TASK-20260622-1000-fixed-timestep-sim-adoption — Verified already-done, review needed 2026-06-22
Verify-first: LocalPlaySimulation already runs on FixedTimestepAccumulator (fixed-dt world.tick + spiral guard) and exposes interpolation alpha plumbed to snapshots. No code changed. Residual: headless/dedicated path still ticks on variable delta. Report: reports/subagents/TASK-20260622-1000-fixed-timestep-sim-adoption-report.md
## TASK-20260622-1010-ecs-migration-first-slice — Verified substantially-done, review needed 2026-06-22
Verify-first: projectiles + dummies already simulated via entt::registry (WorldProjectileComponent/TargetDummyComponent + system fns); fixed arrays remain as a synced view. "Remove array" ripples into net/snapshot/Jolt pointer accessors, so not a clean first slice. No code changed. Report: reports/subagents/TASK-20260622-1010-ecs-migration-first-slice-report.md
## TASK-20260622-1020-deterministic-character-controller — Verified substantially-done, review needed 2026-06-22
Verify-first: deterministic accelerate_movement + MovementConfig + substantive movement_tests already exist on fixed dt. Residual: game.player_* ConfigVars in game_module.cpp are not wired into MovementConfig (tuning not actually hot-reloadable). No code changed. Report: reports/subagents/TASK-20260622-1020-deterministic-character-controller-report.md
## TASK-20260622-1100-phase4-reconciliation-replay-fix — Review needed 2026-06-22
Phase 4 #3: removed the `last_ack_ != 0` guard in ClientPredictionManager::reconcile so unacked inputs replay on the first snapshot too (was dropping pre-snapshot inputs). Added test_first_snapshot_reconciliation (asserts authoritative-then-replay; distinguishes fixed vs buggy). Build (debug) + 10/10 tests pass. Report: reports/subagents/TASK-20260622-1100-phase4-reconciliation-replay-fix-report.md
## TASK-20260622-1110-phase4-reliable-channel — Review needed 2026-06-22
Phase 4 #1: added header-only ae::ReliableChannel (buffer reliable packets by seq; on_ack removes acked incl. bitfield + 16-bit wraparound; collect_retransmits returns timed-out unacked seqs + refreshes). New ahamkara_reliable_channel_tests; build (debug) + 11/11 tests pass. Not yet wired into live client/server loops (follow-up). Report: reports/subagents/TASK-20260622-1110-phase4-reliable-channel-report.md
## TASK-20260622-1200-phase4-netcode-milestone-review — Completed 2026-06-22
Accepted the Phase 4 milestone review: the reconciliation replay fix and reliable channel are complete, and the remaining live-loop socket integration remains a separate follow-up. Report: reports/subagents/TASK-20260622-1200-phase4-netcode-milestone-review-report.md
## TASK-20260622-1020-deterministic-character-controller — Completed 2026-06-22
Accepted the hot-reloadable movement-tuning slice: `game.player_*` ConfigVars now drive the runtime movement path, defaults preserve prior behavior, and the movement config wiring test passes. Review: reports/subagents/TASK-20260622-1020-deterministic-character-controller-codex-review.md
## TASK-20260622-1300-ai-navgrid-astar — Completed 2026-06-22
Accepted the first AI nav slice: `NavGrid` plus deterministic A* are in place and headless-tested. Reviewed as part of the AI nav/movement milestone. Report: reports/subagents/TASK-20260622-1300-ai-navgrid-astar-report.md
## TASK-20260622-1310-ai-path-follower — Completed 2026-06-22
Accepted the second AI nav slice: `PathFollower` and grid-to-waypoint conversion are implemented and tested. Reviewed as part of the AI nav/movement milestone. Report: reports/subagents/TASK-20260622-1310-ai-path-follower-report.md
## TASK-20260622-1320-ai-navgrid-from-collision — Completed 2026-06-22
Accepted the third AI nav slice: collision rectangles now rasterize into a `NavGrid`, and the pathfinding foundation is complete. Reviewed as part of the AI nav/movement milestone. Report: reports/subagents/TASK-20260622-1320-ai-navgrid-from-collision-report.md
## TASK-20260622-1330-milestone-review-ai-nav-and-movement — Completed 2026-06-22
Accepted the AI nav/movement milestone review: movement tuning and the nav foundation are complete, with the batch promoted to `completed/`. Report: reports/subagents/TASK-20260622-1330-milestone-review-ai-nav-and-movement-report.md
## TASK-20260622-1010-ecs-migration-first-slice — Review needed (re-submit after revise) 2026-06-23
API-change pass on the reopened ECS slice. Migrated PROJECTILES (the least-migrated type: its array sync was a no-op, count stuck at 0) fully onto entt::registry: removed `projectiles_[]`/`projectile_count_`, dead `set_projectile_count`/`projectiles_mut`, and the no-op `sync_projectiles_to_array`; `get_projectiles()` now projects `std::vector<ProjectileState>` from the registry view; updated snapshot/forwarder consumers. Dummies intentionally untouched (their pointer accessor is depended on by unmodified world_tests). Tests unmodified; 12/12 pass via debug preset incl. ahamkara_world_tests. NOTE: required `debug-headless` test run is blocked by a PRE-EXISTING `ae_render` link gap (render lib is GL/GLFW-gated, not built headless, but linked unconditionally) — not caused by this change. Restored projectile replication path is runtime-confirm-pending. Report: reports/subagents/TASK-20260622-1010-ecs-migration-first-slice-report.md
## TASK-20260622-1010-ecs-migration-first-slice — Completed 2026-06-23
Accepted the ECS migration slice after the second pass: projectile state is registry-authored with the original pointer/count accessors preserved, the fixed array is gone, and the headless link gap is now guarded so the required debug-headless validation passes. Report: reports/subagents/TASK-20260622-1010-ecs-migration-first-slice-report.md
## TASK-20260620-1415-blender-headless-level-generator — Completed 2026-06-23
Accepted the headless Blender authoring path after the documented Blender 5.1.2 command produced `.blend`, `.gltf`, and byte-identical `.lvl` output for both prototype specs. Report: reports/subagents/TASK-20260620-1415-blender-headless-level-generator-report.md
## TASK-20260622-1010-ecs-migration-first-slice — Review needed (revision 2: accessor shape + headless) 2026-06-23
Addressed Codex revise. (1) Restored the original accessor shape: get_projectiles() is again const ProjectileState* and get_projectile_count() is int, now backed by a DYNAMIC std::vector<ProjectileState> projectiles_ projection refreshed per tick from the registry view (sync_projectiles_to_array()) — fixed array stays removed, registry remains authoritative, consumer call sites reverted. (2) Fixed the pre-existing debug-headless ae_render link gap: guarded ahamkara_game's PRIVATE ae_render (and smoke_tests') with if(TARGET ae_render) — game self-compiles the GL-free compiled_level.cpp and needs no other render symbols. Both lanes now pass: debug-headless 10/10 (the requested validation, now unblocked) and debug 12/12; tests unmodified. Residual: restored projectile population is runtime-visible and not display-confirmed. Report: reports/subagents/TASK-20260622-1010-ecs-migration-first-slice-report.md
## Level-authoring blocked cluster (1400/1415/1500/1510/1520) — Regression-validated on HEAD, still display/Blender-gated 2026-06-23
Reviewed all 5 blocked tasks. Each already has its HEADLESS implementation done + committed (1400 spec+spec_to_lvl emitter; 1415 bpy-free build_level module + unit test; 1500 textured_cube/cube_albedo/cube.mat + textured_showcase level; 1510 DebugRenderer level-environment sky/ambient/fog wiring in debug_renderer + debug_client). Every remaining acceptance item is GL-display-gated; 1415 additionally needs Blender installed. These were built on checkpoint 43ba9cd, so re-validated on current HEAD 926d7a5: spec_to_lvl --selftest 3/3 OK (prototype_arena/prototype_box/textured_showcase); tools/blender/test_build_level passed; asset importer on manifest = Imported 0, skipped 8, failed 0; cmake --build --preset debug clean; ctest 12/12 (incl. asset_pipeline_tests, level_render_tests). No regressions — cluster is display-ready. No code changed and no queue moves (per user direction: leave blocked). Remaining work belongs to TASK-20260620-1520 (runtime-confirm-prototype-levels, requires_display) on a worker/human with a display (+Blender for 1415 execution).
## TASK-20260620-1415-blender-headless-level-generator — Block cleared, review needed 2026-06-23
Blender now installed (5.1.2, /Applications/Blender.app). Ran the documented headless command on two specs: `blender -b -P tools/blender/build_level.py -- assets/levels/{prototype_box,prototype_arena}.json <out>`. Each produced .blend + .gltf (+.bin) + .lvl with NO operator/arg fixups (script authored vs 4.x, ran clean on 5.1.x). Acceptance bar met: the Blender-written .lvl is byte-identical to the Path A spec_to_lvl output for both specs (shared writer); bpy-free unit test passes; cmake debug + ctest 12/12. Verification only, no code changed. Moved blocked -> review-needed. The other level tasks (1400/1500/1510/1520) still need a GL display. Report: reports/subagents/TASK-20260620-1415-blender-headless-level-generator-report.md
## TASK-20260623-1700-debug-client-frozen-view-investigation — Review needed 2026-06-25
Matrix-stack compatibility is fully removed, the gameplay/menu boundary now reads from `ClientMenuState`, and the client/controller-mapper build cleanly with targeted tests green. Runtime display confirmation remains pending in this headless environment. Report: reports/subagents/TASK-20260623-1700-debug-client-frozen-view-investigation-report.md

## TASK-20260704-1000-weapon-runtime-foundation — Review needed 2026-07-04
Hardened WeaponRuntime seam with explicit subclass contract, on_fire() hook, and reload_timer() accessor; wired on_fire() through Player/World into firing paths; confirmed WeaponModelCache is read-only from gameplay; clarified Player weapon-ownership docs. Build + 17/17 tests pass. Report: reports/subagents/TASK-20260704-1000-weapon-runtime-foundation-report.md
## TASK-20260704-1210-weapon-animation-layers — Review needed 2026-07-08
Extracted weapon animation layers (sway, bob, recoil kick) from monolithic evaluate_weapon_animation() into composable standalone functions. Each layer produces an independent render::Mat4 offset. Added notify_fired() to WeaponAnimationController for external recoil-trigger hookup. Backward-compatible wrapper preserved. Build (debug) + world/movement/gameplay/weapon-loader tests pass (4/4). Pre-existing client compile failures block full run-tests.sh. Report: reports/subagents/20260704-1210-weapon-animation-layers-report.md

## 2026-07-05 — agent/phase3/combat-core

Three combat core tasks completed:
1. **TASK-20260704-1010-weapon-fire-control** — Data-driven RPM/reload/reserve from WeaponDefinition; deterministic recoil/spread; removed ad-hoc fire intervals
2. **TASK-20260704-1020-combat-hit-resolution** — Centralized damage resolution and feedback helpers; removed duplicated armor absorption logic
3. **TASK-20260704-1030-combat-abilities-core** — Player-owned AbilityState with cooldowns, energy, ultimate; HUD pipeline wired through snapshot

Report: docs/reports/subagents/TASK-20260704-1010-1020-1030-combat-core-report.md
Build: success | Tests: 17/17 passed
## Phase 5: Animation & Sensory Polish (TASK-1200/1210/1220/1230) — Review needed 2026-07-05
TASK-1200: Character animation runtime wiring via AnimationAdapter bridge, animation_render_bridge fix, joint matrices in DebugScene.
TASK-1210: Weapon animation layers with per-weapon profiles (AR-15, Shotgun, RL) and melee three-phase swing.
TASK-1220: Audio subsystem upgrade with 3D spatialization, weapon/foley/ambience buses, occlusion support.
TASK-1230: VFX feedback system with screen shake, damage flash, hit-reaction triggers, melee UI state.
Build (debug) clean; 14/14 test suites pass. Branch: agent/phase5/sensory-polish. Report: reports/subagents/phase5-sensory-polish.md
|Hardened WeaponRuntime seam with explicit subclass contract, on_fire() hook, and reload_timer() accessor; wired on_fire() through Player/World into firing paths; confirmed WeaponModelCache is read-only from gameplay; clarified Player weapon-ownership docs. Build + 17/17 tests pass. Report: reports/subagents/TASK-20260704-1000-weapon-runtime-foundation-report.md
## TASK-20260704-1400-spatial-partitioning — Self-validated 2026-07-05
|World-scale spatial partitioning: SpatialGrid (uniform 2D grid, AABB/frustum query), OcclusionPortal/PVSRegion data shapes, 6 tests. Build + 19/19 tests pass. Report: reports/subagents/TASK-20260704-1400-spatial-partitioning-report.md
## TASK-20260704-1510-ai-combatants — Self-validated 2026-07-05
|AI combatants: 6 archetypes (Grunt/Sniper/Rusher/Support/Scout/Brute), perception (LOS/FOV/alertness), 7-state behavior FSM, ECS system entry point. 11 tests. Build + 19/19 tests pass. Report: reports/subagents/TASK-20260704-1510-ai-combatants-report.md
## TASK-20260704-1520-encounter-scripting — Self-validated 2026-07-05
|Encounter scripting: EncounterDef/TriggerDef/SpawnWaveDef/ObjectiveDef, EncounterManager with full lifecycle (wave spawn, trigger eval, objective tracking), 9 tests. Build + 19/19 tests pass. Report: reports/subagents/TASK-20260704-1520-encounter-scripting-report.md
## TASK-20260704-1920-viewmodel-arm-hand-ik — Implemented (not validated) 2026-07-06
|||Viewmodel arm/hand IK: per-weapon grip socket transforms, two-bone IK solver (shoulder→elbow→hand) in presentation layer, wired into client_frame_pipeline, debug IK visualization. Build + test validation skipped (cmake not on PATH). Report: reports/subagents/TASK-20260704-1920-viewmodel-arm-hand-ik-report.md
|## TASK-20260704-1930-reload-animation-sequence — Implemented (not validated) 2026-07-06
||Phase-driven reload animation: 4-phase sequence (GrabMag→RemoveMag→InsertMag→ReturnToGrip) driven by WeaponRuntime::reload_timer(), per-weapon reload data (AR-15/Shotgun/RL), hand IK offset to magazine position, weapon tilt during reload phases, data-driven phase timing. Build + test validation skipped (GLFW not available in headless env). Branch: agent/viewmodel/1930-reload. Report: reports/subagents/TASK-20260704-1930-reload-animation-sequence-report.md

## 2026-07-05 — TASK-20260704-1350-legacy-gl-retirement

- **Agent**: opencode
- **Task**: Retire remaining fixed-function GL compatibility seams (APPLE-suffixed VAO extensions, legacy `<OpenGL/gl.h>` header)
- **Files changed**: `gl_platform.h`, `debug_renderer.cpp`, `render_backend.h`
- **Validation**: debug build + 17/17 tests pass
- **Report**: `docs/reports/subagents/TASK-20260704-1350-legacy-gl-retirement-report.md`

## TASK-20260704-1900-viewmodel-offset-tuning — Completed 2026-07-08

- **Agent**: oz
- **Task**: Wire per-weapon position/rotation/FOV viewmodel offsets from `kWeaponViewmodelTransforms` into `DebugScene` fields in `debug_scene_bridge.cpp`.
- **Files changed**: `client/src/debug_scene_bridge.cpp`
- **Validation**: Clean syntax check via full project flags; all game tests pass (world: pass, movement: pass, gameplay: pass, collision: pass)
- **Report**: `docs/reports/subagents/1900-viewmodel-offset-tuning-report.md`

## TASK-20260704-1230-vfx-feedback — Implemented 2026-07-08

VFX feedback: fixed snapshot chain for damage flash, particles, and decals; implemented `draw_damage_flash_overlay()` (red vignette) in HUD; wired world values through `local_play.cpp` and `threaded_local_runtime.cpp`. Build (debug) clean; 16/16 tests pass (world/gameplay/movement). Branch: agent/oz/vfx-feedback. Report: docs/reports/subagents/TASK-20260704-1230-vfx-feedback-report.md

## 2026-07-08 — TASK-20260704-1910-per-weapon-viewmodel-meshes

- **Agent**: oz
- **Task**: Wire each weapon index to its correct compiled viewmodel mesh path (per-weapon viewmodel meshes)
- **Files changed**: Verified already-implemented on main (commit 8b89a5d4) — `weapon_viewmodel_mesh_path()` returns per-weapon paths
- **Validation**: debug build + 20/20 tests pass (includes ahamkara_weapon_loader_tests verifying archetype mesh names)
|- **Report**: `docs/reports/subagents/TASK-20260704-1910-per-weapon-viewmodel-meshes-report.md`

## TASK-20260704-1700-job-system-frame-allocator — Review needed 2026-07-08

Improved job system (condition variable wake-up, dispatch(), submit_after_all()) and frame allocator (ring-buffer, multi-slot). 20 new core_tests (12 job system + 8 frame allocator). Build (debug) + all available test suites pass. Pre-existing admin_server build failure blocks full run-tests.sh.
- **Agent**: oz
- **Branch**: agent/oz/job-system-frame-allocator
- **Files changed**: `job_system.h`, `job_system.cpp`, `frame_allocator.h`, `frame_allocator.cpp`, `tests/src/core_tests.cpp` (new), `tests/CMakeLists.txt`
- **Report**: `docs/reports/subagents/TASK-20260704-1700-job-system-frame-allocator-report.md`
