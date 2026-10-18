# 03 — Rendering & Graphics Engineer

Audit date: 2026-10-16 (America/Chicago)
Repository: `/Users/taufeeqali/Projects/Ahamkara`
Observed HEAD: `b705d74` (`feat: add initial weapon and enemy assets (#71)`)
Observed branch: `dev/flashback-menu-shell`

## 1. Scope & Method

Read `audit/01-recon.md` first. Inspected current renderer interfaces and implementation, CMake gating, PBR/shadow/SSAO/TAA passes, level and material loading, debug rendering, UI module, frame pipeline and the game MCP capture path. Searched renderer public headers and source for native sprite/tilemap and screenshot/readback APIs. Findings describe the working tree observed on this date. Per recon, the checkout has 182 preexisting dirty/untracked entries; renderer API/pipeline, PBR, level rendering, UI, client frame pipeline and MCP bridge files include dirty changes. Findings that touch those files are therefore **working-tree observations, not clean-HEAD attribution**. No GUI run or visual output validation was performed; the recon reports headless-only validation and no display-capable session.

## 2. Executive Summary

**Rendering rating: 2/5 — substantial 3D feature surface, incomplete or disconnected advertised effects, and no demonstrated native 2D scene pipeline.** The current tree contains a real OpenGL renderer, glTF/compiled asset loaders, level mesh submission, PBR-style lighting, cascaded shadow code, debug primitives/effects, and immediate-mode UI. The breadth is ahead of the repo README's “tiny debug renderer” description, but feature names overstate end-to-end implementation: normal maps are not consumed by the PBR fragment shader, TAA is projection jitter and matrix bookkeeping without a history/resolve stage, and SSAO is not initialized or invoked by `PbrRenderer`. The render backend interface is also not presently backend-independent in practice: PBR and post-processing issue raw OpenGL calls, and OpenGL is the only implementation found.

Rendering is a client-gated CMake module (`CMakeLists.txt:169-177`), so the recon's headless build/test run did not behaviorally validate the graphical path. Current features and bugs in dirty renderer files cannot be attributed to `b705d74` without a clean-baseline diff/build. A dirty-tree-only game MCP path does capture the presented client framebuffer to PPM on request, which is useful for agent verification, but it is feature-gated and is not a general render-backend screenshot API.

## 3. Inventory

| Area | Status | Quality (1-5) | Evidence and notes |
|---|---|---:|---|
| Renderer build/runtime | IMPLEMENTED, client-only | 3 | `ae_render` includes the GL backend, PBR, level rendering, debug renderer, and pass sources (`engine/render/CMakeLists.txt:1-29`); root CMake adds render only when `AHAMKARA_BUILD_CLIENT` is on (`CMakeLists.txt:169-177`). |
| 3D mesh and level pipeline | IMPLEMENTED, with limitations | 3 | `LevelRenderScene` loads compiled meshes/materials/textures, builds transforms, chooses LOD, and submits sorted calls (`engine/render/include/ae/render/level_render.h:90-122,145-180`; `engine/render/src/level_render.cpp:135-197,437-443`). glTF and compiled asset loaders are present (`engine/render/include/ae/render/gltf_loader.h`; `compiled_mesh.h`, `compiled_material.h`, `compiled_texture.h`). |
| PBR materials and lights | PARTIAL | 2 | PBR-style shader supports direct lighting, ambient, emissive maps, reflection probes, fog and grading (`engine/render/src/pbr_renderer.cpp:258-389`); normal-map sampler/uniform are declared but not applied. Spot is represented in the enum but follows point-light shader logic (`pbr_renderer.h:14-28`; `pbr_renderer.cpp:272-286`). |
| Shadows | PARTIAL / implemented path | 3 | Shadow pass exposes single and cascaded shadow rendering (`engine/render/include/ae/render/shadow_pass.h:22-40`); PBR builds and uploads cascade matrices when a shadow pass is supplied (`engine/render/src/pbr_renderer.cpp:555-599`). No live image verification performed. |
| SSAO and TAA | PARTIAL / disconnected | 1 | SSAO has its own GL pass (`engine/render/src/ssao_pass.cpp:15-33,136-165`) but no calls to initialize/render it were found in `PbrRenderer`; TAA stores matrices and jitters projection only (`engine/render/src/temporal_aa.cpp:13-57`). |
| Camera and visibility | IMPLEMENTED, limited | 3 | Level rendering exposes distance-based LOD and frustum utilities (`engine/render/include/ae/render/level_render.h:25-35,103-109`; `engine/render/include/ae/render/frustum.h`). Client/game provide camera state (`game/src/world_camera.cpp:12-53`). |
| Debug geometry, particles, decals | IMPLEMENTED for debug/product path | 3 | `DebugScene` carries bounded particle/decal state and render stats (`engine/render/include/ae/render/debug_renderer.h:141-164`); debug renderer dispatches particles (`engine/render/src/debug_renderer.cpp:2003-2005`) and draws them via its effect path (`engine/render/src/debug_renderer_effects.cpp:13-92`). |
| UI/HUD/menu | IMPLEMENTED as separate immediate-mode UI module | 3 | `ae_ui` compiles Dear ImGui plus project menu/HUD systems and links to renderer/platform (`engine/ui/CMakeLists.txt:1-29`); HUD and menu own their draw-list rendering. This is UI composition, not evidence of a native general-purpose sprite renderer. |
| Native sprite/tilemap scene pipeline | NOT FOUND | 1 | No sprite batch or tilemap types/API found in `engine/render/include/ae/render`; `LevelAsset` contains spawn points, collision geometry and 3D mesh instances only (`engine/render/include/ae/render/compiled_level.h:30-75`). UI image quads do not provide scene sprites or tilemaps. |
| Screenshot/readback for AI verification | PARTIAL, dirty-tree only | 3 | Client frame pipeline calls `glReadPixels` on a token-authenticated request and writes a PPM (`client/src/client_frame_pipeline.cpp:1005-1021`; `client/src/game_mcp_bridge.cpp:298-342`). Both files are dirty; compile/runtime activation requires the game MCP feature gate. |
| Alternative graphics backends | NOT IMPLEMENTED | 1 | The declared interface mentions future backends, but the only concrete backend is `OpenGLRenderBackend` (`engine/render/include/ae/render/render_backend.h:85-95`; `engine/render/src/render_backend_opengl.cpp:54-83`). Several passes use GL directly, bypassing the interface. |

## 4. Detailed Findings

### REND-001 — TAA does not perform a temporal resolve and uses a fixed render size

- **Severity:** HIGH
- **Category:** rendering / implementation gap
- **Evidence:** `TemporalAA::initialize` ignores backend and dimensions; the class stores only previous view/projection matrices and `has_history_` (`engine/render/src/temporal_aa.cpp:13-23,53-57`; `engine/render/include/ae/render/temporal_aa.h:8-33`). `PbrRenderer::begin_frame` hard-codes `1920x1080` for jitter (`engine/render/src/pbr_renderer.cpp:492-501`). PBR uploads previous matrices, but the fragment shader has no history texture or temporal resolve; the uniforms are declared and located, while the end-frame method only stores matrices and increments the index (`pbr_renderer.cpp:199-201,472-474,747-751`).
- **Impact:** The present implementation jitters projection coordinates but cannot accumulate samples or reject history, so it is not temporal anti-aliasing as normally expected. Jitter scale is wrong at non-1920x1080 framebuffer sizes and can produce inconsistent image motion.
- **Recommended fix:** Either rename/document this as projection jitter, or add a render-target history and resolve pass with motion/reprojection data. Pass actual framebuffer dimensions into begin-frame/jitter and test multiple sizes. Because `pbr_renderer.cpp` is dirty, first compare against the intended working changes before implementation or attribution.
- **Effort:** M
- **Dependencies:** renderer frame graph and framebuffer-size ownership.

### REND-002 — SSAO pass is not connected to the main PBR frame path

- **Severity:** HIGH
- **Category:** rendering / implementation gap
- **Evidence:** `PbrRenderer::Impl` owns `SsaoPass`, but its only usage in `pbr_renderer.cpp` is shutdown (`:62,735`). No initialization or render call exists in that file; ambient occlusion is consumed only when a caller externally supplies `ao_texture_` (`:623-648,753-759`). The standalone pass does allocate an AO target and implements a depth-sampling draw (`engine/render/src/ssao_pass.cpp:15-33,136-165`).
- **Impact:** Enabling the renderer's SSAO flag cannot generate SSAO itself; absent an external AO texture, the feature is inert. Public class/API presence may mislead users into expecting an integrated effect.
- **Recommended fix:** Wire pass initialization, depth input, execution order, and result binding into the frame graph, with resize/context lifecycle handling; otherwise mark SSAO as externally provided AO and avoid advertising a working integrated SSAO feature.
- **Effort:** M
- **Dependencies:** depth texture ownership and render-pass ordering.

### REND-003 — PBR material maps and authored material properties are not fully honored

- **Severity:** HIGH
- **Category:** rendering / asset fidelity
- **Evidence:** The PBR fragment shader declares `uNormalMap` and `uHasNormalMap`, yet computes `N` exclusively as `normalize(vNormal)` and never samples the normal map (`engine/render/src/pbr_renderer.cpp:174-180,258-263`). Level material conversion copies base RGB, metallic and roughness only (`engine/render/src/level_render.cpp:100-108`). `make_level_draw_call` hard-codes emissive color and intensity to zero (`level_render.cpp:214-230`), while `MaterialAsset` exposes base alpha, emissive RGB and double-sided fields (`engine/render/include/ae/render/compiled_material.h:8-23`).
- **Impact:** Normal-mapped assets render without authored normal detail. Authored material emissive RGB, base alpha and double-sided behavior are lost in level submission. This undermines visual consistency and any image-based asset verification.
- **Recommended fix:** Carry supported material fields through `MaterialAsset` → `LevelRenderInstance` → `PbrDrawCall`; apply tangent-space normal maps and define alpha/double-sided blend/cull behavior, or reject unsupported fields explicitly during import. The PBR and level-render files are dirty, so confirm these observations against the intended diff before attributing them to branch history.
- **Effort:** M
- **Dependencies:** tangent data in compiled meshes and explicit alpha/culling policy.

### REND-004 — Renderer abstraction does not currently isolate graphics APIs

- **Severity:** MEDIUM
- **Category:** architecture / maintainability
- **Evidence:** `RenderBackend` is described as backend-agnostic and future Vulkan/Metal/D3D implementations, but only one implementation, `OpenGLRenderBackend`, is present (`engine/render/include/ae/render/render_backend.h:85-95`; `engine/render/src/render_backend_opengl.cpp:54-57`). `PbrRenderer` includes GL platform headers and issues direct GL calls for VAOs, uniforms, vertex attributes and draw calls (`engine/render/src/pbr_renderer.cpp:7,398-399,503-510,694-731`); SSAO and shadow passes also use raw GL (`engine/render/src/ssao_pass.cpp:3,15-33`; `engine/render/src/shadow_pass.cpp`). The backend header itself is dirty.
- **Impact:** New graphics backends would need to duplicate or redesign substantial pass behavior; swapping backend implementations cannot be done by implementing the current interface alone. The interface therefore overstates portability.
- **Recommended fix:** Choose and document a near-term target: either explicitly scope the engine to OpenGL, or move shader/pass/framebuffer/vertex binding operations behind backend-owned commands and resources before promising alternate APIs.
- **Effort:** L
- **Dependencies:** engine platform targets and graphics backend roadmap.

### REND-005 — No engine-native 2D sprite or tilemap rendering path was found

- **Severity:** HIGH
- **Category:** capability gap / product strategy
- **Evidence:** The renderer's public headers cover meshes, levels, materials, textures, debug geometry, cameras/visibility and post-processing, but contain no sprite-batch or tilemap abstraction (`engine/render/include/ae/render/` inventory). `LevelAsset` models 3D `LevelMeshInstance` and collision geometry, not sprite/tilemap layers (`engine/render/include/ae/render/compiled_level.h:30-75`). UI images are drawn by the menu system through ImGui draw lists (`engine/ui/src/menu_system.cpp:405-435`).
- **Impact:** Current evidence supports a 3D-first engine with UI overlays, not a general-purpose 2D/3D engine. 2D games would need to build a bespoke renderer or repurpose UI drawing, which lacks scene-camera, batching, sorting, atlas and tilemap semantics.
- **Recommended fix:** If 2D games are in target scope, add an explicit camera-space sprite batch, texture atlas/sampler contract, tilemap asset/runtime representation, and 2D scene tests. If not, document the supported game scope.
- **Effort:** L
- **Dependencies:** owner decision on target game genres/platforms.

### REND-006 — AI screenshot capture exists only as a feature-gated client bridge

- **Severity:** MEDIUM
- **Category:** AI verification / tooling
- **Evidence:** Under `AHAMKARA_ENABLE_GAME_MCP`, `ClientFramePipeline::stage_present` checks a capture request, reads the GLFW framebuffer with `glReadPixels`, and delegates PPM output (`client/src/client_frame_pipeline.cpp:1005-1021`). `GameMcpBridge` requires an enabled environment configuration with root and token (`client/src/game_mcp_bridge.cpp:92-109`), validates the request token, writes P6 PPM, flips rows, and removes the request (`:298-342`). `client/src/client_frame_pipeline.cpp` and `client/src/game_mcp_bridge.cpp` are dirty in the observed tree, so this is not confirmed as a committed capability.
- **Impact:** The current tree has a useful external agent hook for visual inspection, but it is client-only, compile-time gated, filesystem-protocol based, and tied directly to GLFW/OpenGL. The core renderer cannot provide a screenshot through its backend contract, and no graphical capture was exercised in this audit.
- **Recommended fix:** Keep this bridge documented as development tooling and expose a stable capture contract (frame dimensions, pixel format/orientation, output error reporting and deterministic timing). Add an automated headless/offscreen or display-capable smoke path before treating it as reliable visual regression evidence.
- **Effort:** M
- **Dependencies:** game MCP configuration and a graphical/offscreen CI environment.

## 5. What's Good

- The engine has a distinct render library with a clear collection of core rendering, asset-loading, PBR, debug, profiling and effect sources (`engine/render/CMakeLists.txt:1-29`).
- `LevelRenderScene` separates CPU-side level data from GPU resources and supports optional LOD assets, material maps, transform composition, and mesh/material ordering (`engine/render/include/ae/render/level_render.h:90-133,145-180`; `engine/render/src/level_render.cpp:135-197,239-365`).
- `DebugScene` gives product/debug rendering explicit bounded state for particles, decals, hit feedback, metrics and camera/viewmodel data; particle rendering is split into an effects source (`engine/render/include/ae/render/debug_renderer.h:28-223`; `engine/render/src/debug_renderer_effects.cpp:13-92`).
- The immediate-mode UI is packaged separately from `ae_render`, and CMake keeps render/UI/client dependencies behind the client build option (`engine/ui/CMakeLists.txt:1-29`; `CMakeLists.txt:169-177`).
- The dirty working tree's MCP capture path reads after frame rendering and before buffer swap, writes RGB PPM with top-to-bottom row correction, and protects requests with a configured token (`client/src/client_frame_pipeline.cpp:1005-1021`; `client/src/game_mcp_bridge.cpp:298-342`).

## 6. Open Questions

- Are 2D games, isometric games, or tile-based worlds within the target scope? Recon recommends assessing demonstrated implementation and deferring genre prioritization until owner input.
- Is OpenGL the intended long-term graphics API, or should the backend-neutral `RenderBackend` contract be treated as a concrete portability goal?
- Should TAA and SSAO be treated as desired user-facing renderer features, or as partially authored experiments pending integration?
- Is the current game MCP capture bridge part of the intended uncommitted work and expected AI verification workflow? It is dirty and not confirmed on clean HEAD.

## 7. Handoff Notes

- Preserve the dirty tree; do not reset, clean, stash, checkout, or infer these changes are part of `b705d74`. Renderer findings from dirty files need attribution only after owner identifies the intended baseline.
- No graphical demo was launched and no rendered output was inspected. The recon's build validation was headless, and client-gated renderer code is outside that graph.
- High-priority engineering follow-up should be to establish a visual baseline on a known checkout, then close or accurately label the TAA/SSAO and material-map gaps before using screenshots as quality evidence.
- Verify source references against the current tree when changes land; this report is a snapshot, not a replacement for implementation tests or image-based validation.
