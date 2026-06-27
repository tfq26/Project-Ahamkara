# Renderer Backend Abstraction

## Status: First Backend Abstraction Implemented

## Overview

The renderer now has a `RenderBackend` abstraction (`ae/render/render_backend.h`)
that decouples GPU resource management and draw submission from rendering logic.

A single OpenGL implementation (`render_backend_opengl.cpp`) provides the
backend today. Future Vulkan, Metal, or D3D12 backends can implement the same
interface.

## Architecture

```text
┌─────────────────────────────────────────────────────────┐
│  DebugRenderer (debug_renderer.cpp)                     │
│  ┌───────────────────────────────────────────────────┐  │
│  │ Scene traversal, culling, LOD, HUD, sky,          │  │
│  │ immediate-mode debug shapes                       │  │
│  └──────────┬──────────────────────────┬─────────────┘  │
│             │ Backend-agnostic         │ Legacy GL       │
│             │ (buffers, shaders,       │ (glBegin/End,   │
│             │  meshes, queries, draw)  │  matrix stack,  │
│             ▼                          │  lighting)      │
│  ┌──────────────────────┐              ▼                 │
│  │ RenderBackend        │    Direct OpenGL calls        │
│  │ (abstract interface) │                                │
│  └──────────┬───────────┘                                │
│             │                                            │
│  ┌──────────▼───────────┐                                │
│  │ OpenGLRenderBackend  │                                │
│  │ (render_backend_     │                                │
│  │  opengl.cpp)         │                                │
│  └──────────────────────┘                                │
└─────────────────────────────────────────────────────────┘
```

## RenderBackend Interface

```cpp
class RenderBackend {
    // --- Lifecycle ---
    bool initialize(void* native_window_handle);
    void shutdown();

    // --- Frame ---
    void begin_frame();
    void end_frame();  // swap / present
    void get_framebuffer_size(int& w, int& h);

    // --- Core state ---
    void set_viewport(...);
    void set_clear_color(...);
    void clear_color_and_depth();
    void set_depth_test(bool);
    void set_depth_func_less/equal/lequal();
    void set_depth_write(bool);
    void set_color_write(bool r, g, b, a);

    // --- Buffers ---
    BufferHandle create_vertex_buffer(data, size, dynamic);
    BufferHandle create_index_buffer(data, count);
    void destroy_buffer(handle);
    void update_vertex_buffer(handle, data, size, offset);

    // --- Shaders ---
    ShaderHandle create_shader_program(desc);
    void destroy_shader(handle);
    void use_shader(handle);
    int  get_uniform_location(handle, name);
    void set_uniform_*(...);

    // --- GPU meshes ---
    GpuMesh  create_gpu_mesh(cpu_mesh);
    GpuModel create_gpu_model(cpu_model);
    void destroy_gpu_mesh(mesh);
    void destroy_gpu_model(model);

    // --- Draw ---
    void draw_gpu_mesh(mesh);
    void draw_gpu_mesh_skinned(mesh);
    void draw_arrays_vnc(pos, nrm, col, first, count);
    void draw_arrays_positions(pos, first, count);

    // --- Queries ---
    QueryHandle create_query(is_timer);
    void destroy_query(handle);
    void begin_query / end_query(handle);
    bool is_query_result_available(handle);
    uint32/uint64 get_query_result(handle);
    bool timer_queries_supported();
};
```

## Resource Handle Design

All GPU resource handles are opaque `{ uint32_t id; }` structs.  The OpenGL
backend maps `id` values to raw GL object names through an internal handle pool.
This means no GL headers leak through the public interface.

```cpp
BufferHandle   // VBO, IBO
ShaderHandle   // compiled + linked shader program
QueryHandle    // timer or occlusion query
```

## What Is NOT in the Backend

The following remain as direct GL calls in `debug_renderer.cpp`:

| Feature | Reason |
|---------|--------|
| `glBegin` / `glEnd` immediate mode | Debug-only; replaced by production pipeline |
| Fixed-function lighting (`glLight*`, `glLightModel*`) | Legacy GL 2.1 feature |
| Occlusion queries (raw `GL_SAMPLES_PASSED`) | Legacy; future GPU-driven culling |
| `MapGeometry` cell VBO management | Owned by `map_geometry.cpp`; migrates later |
| Dynamic VBO mapping (`glMapBuffer`) for particles | Migrates with particle system refactor |

These are intentionally kept as direct GL because:
1. They represent immediate-mode debug rendering that a production renderer
   would replace entirely.
2. Abstracting them would overbuild the interface without real architectural
   benefit at this stage.

## Adding a New Backend

To add a backend (e.g., Vulkan):

1. Implement `RenderBackend` in a new file (e.g., `render_backend_vulkan.cpp`).
2. Add a factory function (e.g., `create_vulkan_backend()`).
3. Update the render target's `CMakeLists.txt` to link the new backend.
4. At startup, select the backend based on platform / user preference.

The `RenderBackend` interface is purposefully limited to what the debug
renderer currently needs.  As the renderer evolves toward a production FPS
pipeline, the interface will grow — but the seam has been established.

## Public API Changes

| File | Change |
|------|--------|
| `ae/render/render_backend.h` | **New.** Abstract backend interface, `GpuMesh`, `GpuModel`, opaque handles |
| `ae/render/debug_renderer.h` | No API change.  `DebugRenderer` public surface is unchanged. |
| `engine/render/src/render_backend_opengl.cpp` | **New.** OpenGL implementation |
| `engine/render/src/debug_renderer.cpp` | Internal refactor to use backend; no public API change |
| `engine/render/CMakeLists.txt` | Added `render_backend_opengl.cpp` |

No changes to `DebugScene`, `PlatformWindow`, or any client/game code.
