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
