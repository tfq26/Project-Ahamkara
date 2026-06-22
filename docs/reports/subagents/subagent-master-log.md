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
