#pragma once

#include "ae/render/gltf_loader.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace ae::render {

// ---------------------------------------------------------------------------
// Opaque GPU resource handles
// ---------------------------------------------------------------------------

struct BufferHandle {
    std::uint32_t id = 0;
    explicit operator bool() const { return id != 0; }
};

struct ShaderHandle {
    std::uint32_t id = 0;
    explicit operator bool() const { return id != 0; }
};

struct QueryHandle {
    std::uint32_t id = 0;
    explicit operator bool() const { return id != 0; }
};

inline bool operator==(BufferHandle a, BufferHandle b) { return a.id == b.id; }
inline bool operator!=(BufferHandle a, BufferHandle b) { return a.id != b.id; }
inline bool operator==(ShaderHandle a, ShaderHandle b) { return a.id == b.id; }
inline bool operator!=(ShaderHandle a, ShaderHandle b) { return a.id != b.id; }
inline bool operator==(QueryHandle a, QueryHandle b)  { return a.id == b.id; }
inline bool operator!=(QueryHandle a, QueryHandle b)  { return a.id != b.id; }

constexpr BufferHandle kInvalidBuffer{0};
constexpr ShaderHandle kInvalidShader{0};
constexpr QueryHandle  kInvalidQuery{0};

// ---------------------------------------------------------------------------
// Shader program description
// ---------------------------------------------------------------------------

struct ShaderProgramDesc {
    const char*  vertex_source   = nullptr;
    const char*  fragment_source = nullptr;
    const int*   attrib_locations = nullptr;
    const char** attrib_names     = nullptr;
    int          attrib_count     = 0;
};

// ---------------------------------------------------------------------------
// GPU-side mesh (backend-agnostic)
// ---------------------------------------------------------------------------

struct GpuMesh {
    BufferHandle vbo_positions;
    BufferHandle vbo_normals;
    BufferHandle vbo_joints;
    BufferHandle vbo_weights;
    BufferHandle ibo_indices;
    int  index_count  = 0;
    int  vertex_count = 0;
    float color_r = 1.0F;
    float color_g = 1.0F;
    float color_b = 1.0F;
};

struct GpuModel {
    std::vector<GpuMesh> meshes;
};

// ---------------------------------------------------------------------------
// RenderBackend — abstract GPU backend
// ---------------------------------------------------------------------------
//
// Provides resource creation, shader management, and draw calls through a
// backend-agnostic interface.  Each platform window has one backend instance.
//
// The current OpenGL implementation lives in render_backend_opengl.cpp.
// Future Vulkan / Metal / D3D backends implement this same interface.
//
// Immediate-mode debug drawing (glBegin/glEnd, fixed-function lighting,
// matrix stack) is NOT part of this abstraction — those calls remain in
// the debug renderer and are expected to be replaced by production rendering.

class RenderBackend {
public:
    virtual ~RenderBackend() = default;

    RenderBackend(const RenderBackend&) = delete;
    RenderBackend& operator=(const RenderBackend&) = delete;
    RenderBackend(RenderBackend&&) = delete;
    RenderBackend& operator=(RenderBackend&&) = delete;

    // --- Lifecycle ---
    /// Attach to the platform window's native handle and initialise the
    /// GPU context.  Returns false on failure.
    virtual bool initialize(void* native_window_handle) = 0;

    /// Release all GPU resources and detach from the window.
    virtual void shutdown() = 0;

    // --- Frame ---
    /// Called at the start of each frame.
    virtual void begin_frame() = 0;

    /// Swap buffers / present the frame.  Called at the very end.
    virtual void end_frame() = 0;

    /// Query the current framebuffer size in pixels.
    virtual void get_framebuffer_size(int& width, int& height) = 0;

    // --- Viewport & clear ---
    virtual void set_viewport(int x, int y, int w, int h) = 0;
    virtual void set_clear_color(float r, float g, float b, float a) = 0;
    virtual void clear_color_and_depth() = 0;

    // --- Depth state ---
    virtual void set_depth_test(bool enabled) = 0;
    virtual void set_depth_func_less() = 0;
    virtual void set_depth_func_equal() = 0;
    virtual void set_depth_func_lequal() = 0;
    virtual void set_depth_write(bool enabled) = 0;
    virtual void set_color_write(bool r, bool g, bool b, bool a) = 0;

    // --- Buffers ---
    /// Create a vertex buffer with initial data.  If `dynamic` is true the
    /// buffer may be updated frequently (e.g. GL_DYNAMIC_DRAW).
    virtual BufferHandle create_vertex_buffer(const void* data,
                                              std::size_t size_bytes,
                                              bool dynamic) = 0;

    /// Create an index buffer (32-bit indices).  `count` is the number of
    /// std::uint32_t elements.
    virtual BufferHandle create_index_buffer(const std::uint32_t* data,
                                             std::size_t count) = 0;

    /// Destroy a previously created buffer.
    virtual void destroy_buffer(BufferHandle handle) = 0;

    /// Update a dynamic vertex buffer (partial or full).
    virtual void update_vertex_buffer(BufferHandle handle,
                                      const void* data,
                                      std::size_t size_bytes,
                                      std::size_t offset = 0) = 0;

    // --- Shaders ---
    /// Compile and link a shader program.  Returns kInvalidShader on failure.
    virtual ShaderHandle create_shader_program(const ShaderProgramDesc& desc) = 0;

    /// Destroy a shader program.
    virtual void destroy_shader(ShaderHandle handle) = 0;

    /// Bind a shader for subsequent draw calls.
    virtual void use_shader(ShaderHandle handle) = 0;

    /// Get the location of a named uniform.  Returns -1 if not found.
    virtual int get_uniform_location(ShaderHandle handle, const char* name) = 0;

    virtual void set_uniform_int(int location, int value) = 0;
    virtual void set_uniform_float(int location, float value) = 0;
    virtual void set_uniform_vec2(int location, float x, float y) = 0;
    virtual void set_uniform_vec3(int location, float x, float y, float z) = 0;
    virtual void set_uniform_vec4(int location, float x, float y, float z, float w) = 0;
    virtual void set_uniform_mat4_array(int location,
                                        const float* matrices,
                                        int count) = 0;

    // --- GPU mesh helpers ---
    /// Build GPU buffers from a CPU-side GltfMesh.
    virtual GpuMesh create_gpu_mesh(const GltfMesh& mesh) = 0;

    /// Build a GpuModel from a GltfModel.
    virtual GpuModel create_gpu_model(const GltfModel& model) = 0;

    /// Release all GPU buffers owned by a mesh.
    virtual void destroy_gpu_mesh(GpuMesh& mesh) = 0;

    /// Release all GPU buffers owned by a model.
    virtual void destroy_gpu_model(GpuModel& model) = 0;

    // --- Draw calls for GPU meshes ---
    /// Bind a mesh's vertex/index buffers and issue a draw call.
    /// Does NOT bind or unbind any shader program.
    virtual void draw_gpu_mesh(const GpuMesh& mesh) = 0;

    /// Bind the vertex/index buffers for a mesh that has skinning attributes
    /// (joint indices and weights).  Disables the skinning attrib arrays
    /// after drawing.
    virtual void draw_gpu_mesh_skinned(const GpuMesh& mesh) = 0;

    /// Draw using client vertex/normal/color arrays bound to explicit VBOs
    /// (used by MapGeometry cells).  `vbo_positions`, `vbo_normals`,
    /// `vbo_colors` are the buffer handles; `first` and `count` are in
    /// vertices.  Passes kInvalidBuffer to skip a buffer.
    /// `color_components` must be 3 (RGB) or 4 (RGBA).
    virtual void draw_arrays_vnc(BufferHandle vbo_positions,
                                 BufferHandle vbo_normals,
                                 BufferHandle vbo_colors,
                                 int first, int count,
                                 int color_components = 3) = 0;

    /// Draw arrays with only a position VBO (lines, etc.).
    virtual void draw_arrays_positions(BufferHandle vbo_positions,
                                       int first, int count) = 0;

    // --- Queries ---
    /// Create a GPU query object.  `is_timer` = true for GL_TIME_ELAPSED,
    /// false for GL_SAMPLES_PASSED.
    virtual QueryHandle create_query(bool is_timer) = 0;

    /// Destroy a query.
    virtual void destroy_query(QueryHandle handle) = 0;

    /// Begin a query.
    virtual void begin_query(QueryHandle handle) = 0;

    /// End a query.
    virtual void end_query(QueryHandle handle) = 0;

    /// Check whether a query result is available.
    virtual bool is_query_result_available(QueryHandle handle) = 0;

    /// Get the query result as a 32-bit unsigned int (GL_SAMPLES_PASSED).
    virtual std::uint32_t get_query_result_uint(QueryHandle handle) = 0;

    /// Get the query result as a 64-bit unsigned int (GL_TIME_ELAPSED, ns).
    virtual std::uint64_t get_query_result_uint64(QueryHandle handle) = 0;

    // --- Capability queries ---
    /// Whether GL_ARB_timer_query or GL_EXT_timer_query is available.
    virtual bool timer_queries_supported() = 0;

    // --- Swap interval ---
    virtual void set_swap_interval(int interval) = 0;

protected:
    RenderBackend() = default;
};

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

/// Create an OpenGL RenderBackend for the given native window handle.
/// The handle is expected to be a GLFWwindow*.
[[nodiscard]] std::unique_ptr<RenderBackend> create_opengl_backend();

}  // namespace ae::render
