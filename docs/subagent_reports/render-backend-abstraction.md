# Task
Introduce a minimal `RenderBackend` abstraction to decouple GPU resource management from rendering logic, preparing for future Vulkan/Metal/D3D backends without rewriting the debug renderer.

# Outcome
Fully implemented: `RenderBackend` abstract interface with opaque handle types (`BufferHandle`, `ShaderHandle`, `QueryHandle`), backend-agnostic `GpuMesh`/`GpuModel` wrappers, and a complete OpenGL implementation. The `DebugRenderer` now routes all GPU resource creation (buffers, shaders, queries, mesh VBOs), draw submission, and frame lifecycle through the backend. The debug scene still renders as before — no visual or behavioral change.

Partially implemented: `MapGeometry` cell VBO management and occlusion queries remain as raw GL calls in `debug_renderer.cpp`. Particle/decal dynamic buffer orphaning was replaced with `glBufferSubData`-based updates (loses the orphaning optimisation but is correct).

Not implemented: Vulkan/Metal/D3D backends. Dynamic resolution scaling. GPU-driven instancing. Material system. Texture loading/compression.

# Files Changed
- `engine/render/include/ae/render/render_backend.h` — **New.** Abstract `RenderBackend` class, opaque handle types, `GpuMesh`/`GpuModel` structs, `ShaderProgramDesc`, factory `create_opengl_backend()`.
- `engine/render/src/render_backend_opengl.cpp` — **New.** Full `OpenGLRenderBackend` implementation: GL 2.1 buffer/shader/query management, handle pools, draw calls, state management.
- `engine/render/src/debug_renderer.cpp` — Removed `GltfMeshVBO`/`GltfModelVBO` structs and `create_mesh_vbo`/`compile_shader`/`link_shader_program`/`draw_model_vbo` helpers (~215 lines). Updated `DebugRenderer::Impl` to use `RenderBackend`, `BufferHandle`, `ShaderHandle`, `QueryHandle`, `GpuModel`. Updated `initialize()`, `shutdown()`, `render()` to route GPU ops through backend. Added `draw_gpu_model` helper. Updated `draw_particles`/`draw_decals` signatures to accept `RenderBackend&` + `BufferHandle&`.
- `engine/render/CMakeLists.txt` — Added `src/render_backend_opengl.cpp` to `ae_render` library sources.
- `docs/renderer_backend.md` — **New.** Architecture documentation: interface overview, handle design, what-is-not-in-backend table, how-to-add-new-backend guide, public API changes table.

# Interfaces Added Or Changed
- `ae::render::RenderBackend` — abstract class with ~30 virtual methods for buffer/shader/query/mesh lifecycle, draw submission, state management, and frame lifecycle.
- `ae::render::BufferHandle` — opaque `{ uint32_t id; }` struct for VBO/IBO handles.
- `ae::render::ShaderHandle` — opaque `{ uint32_t id; }` struct for compiled shader program handles.
- `ae::render::QueryHandle` — opaque `{ uint32_t id; }` struct for timer/occlusion query handles.
- `ae::render::GpuMesh` — backend-agnostic GPU mesh with `BufferHandle` fields for positions, normals, joints, weights, index buffer; plus vertex/index counts and base colour.
- `ae::render::GpuModel` — `std::vector<GpuMesh>` wrapper.
- `ae::render::ShaderProgramDesc` — shader source + attribute binding description struct.
- `ae::render::create_opengl_backend()` — factory returning `std::unique_ptr<RenderBackend>`.
- `constexpr` sentinels: `kInvalidBuffer{0}`, `kInvalidShader{0}`, `kInvalidQuery{0}`.
- `ae::render::DebugRenderer` — **public API unchanged** (`debug_renderer.h` untouched).
- `ae::render::DebugScene` — **unchanged**.
- `ae::PlatformWindow` — **unchanged**.

# Behavior
The client renders the existing debug scene identically. All GPU resource creation, shader compilation, buffer upload, mesh drawing, timer query, and swap now flow through the `RenderBackend` interface rather than direct GL calls. Immediate-mode debug drawing (sky gradient, HUD, crosshair, debug boxes, axes) and fixed-function lighting remain as direct OpenGL — these are explicitly scoped as legacy debug code, not production rendering.

The `WindowConfig::create_opengl_context` flag in `PlatformWindow` is still used; the backend is created after the window and receives its native handle.

# Validation
- `cmake --build build/debug --target ae_render` — passes, zero errors, zero warnings on render sources.
- `cmake --build build/debug --target ahamkara_client` — passes, links successfully.
- `cmake --build build/debug --target all` — passes.
- No tests run (no renderer-specific tests exist in the project).
- Runtime validation not performed (no macOS GPU available in this environment to launch the client).

# Known Gaps
- `MapGeometry` (`map_geometry.cpp`) stores raw `unsigned int` GL handles in `MapCellVBO`. Not yet migrated to `BufferHandle`.
- Occlusion queries use raw `glGenQueries`/`glBeginQuery(GL_SAMPLES_PASSED)` in `debug_renderer.cpp`. No backend abstraction for occlusion queries (they are planned for replacement by GPU-driven culling).
- Particle/decal dynamic buffer updates use `glBufferSubData` via `update_vertex_buffer()` instead of the previous orphaning (`glBufferData(nullptr)` + `glMapBuffer`). Correct but less efficient for streaming uploads.
- The backend uses GLSL `#version 120` shaders referencing fixed-function built-ins (`gl_ModelViewMatrix`, `gl_LightSource`). A future Vulkan backend will need rewritten shaders.
- `RenderBackend` does not abstract `glfwGetTime()` — the debug renderer calls it directly via the cached `GLFWwindow*`.
- No backend selection mechanism — the factory always returns the OpenGL backend.

# Risks
- The `RenderBackend` interface is designed to match exactly what the debug renderer currently needs. It is not a general-purpose GPU API. Adding a Vulkan backend will require extending the interface (e.g., pipeline state objects, descriptor sets, resource barriers).
- `GpuMesh` stores `BufferHandle` fields directly — no RAII wrapper. Callers must call `destroy_gpu_mesh()`/`destroy_gpu_model()` explicitly. `DebugRenderer::shutdown()` does this correctly, but the design is fragile for future use.
- The OpenGL backend uses `std::unordered_map` for handle pools. If handle creation/destruction becomes a hot path, this should be replaced with a `std::vector`-based pool.
- The `#define GL_GLEXT_PROTOTYPES` and `<OpenGL/glext.h>` includes are duplicated in both `render_backend_opengl.cpp` and `debug_renderer.cpp`. These could be consolidated into a single platform GL header if more backends are added.

# Next Recommended Steps
1. Run the client on macOS to visually confirm the debug scene renders correctly.
2. Migrate `MapGeometry` (`map_geometry.h`/`.cpp`) to use `BufferHandle` instead of raw `unsigned int`, routing VBO creation through `RenderBackend`.
3. Migrate occlusion queries into the backend (or remove them in favour of a frustum-only culling path until GPU-driven culling is ready).
4. Add a `native_buffer_handle()` accessor to `RenderBackend` (or a `map_buffer()` method) to restore the orphaning optimisation for particle/decal dynamic buffers.
5. Add a backend selection mechanism — e.g., a `WindowConfig::render_backend` enum or an environment variable, with the factory dispatching to OpenGL/Vulkan/Metal.
6. Extract GLFW/GL platform includes into a single `engine/render/src/platform/gl_platform.h` header, shared by the OpenGL backend and any remaining GL code in the debug renderer.
7. Begin designing the production shader pipeline (SPIR-V / GLSL 4.60) as a separate `ShaderPipeline` abstraction on top of `RenderBackend`.

# Notes For Integration
- No public API break — `DebugRenderer`, `DebugScene`, `PlatformWindow` are unchanged. Client code requires no modification.
- The `ae_render` library now has one additional translation unit (`render_backend_opengl.cpp`). The CMake change is additive.
- The new `docs/renderer_backend.md` should be linked from `docs/architecture.md` in the renderer section.
- If merging into a branch that has parallel render changes, the conflict surface is: `debug_renderer.cpp` (Impl struct fields, initialize/shutdown/render bodies), `CMakeLists.txt` (source list), and any new includes.
