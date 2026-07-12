#include "ae/render/render_backend.h"

#include "ae/core/log.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "ae/render/gl_platform.h"

// Use the compat layer's core-profile draw helper without remapping this file's
// own (real) GL calls.
#define AE_GL_COMPAT_NO_REMAP
#include "gl_compat.h"

#include <cstdio>
#include <string>
#include <unordered_map>

namespace ae::render {
namespace {

// ---------------------------------------------------------------------------
// Internal handle allocator
// ---------------------------------------------------------------------------
// Simple monotonic allocator so opaque Handle::id values are dense and
// never alias across resource types (each type gets its own pool).

template <typename NativeType>
class HandlePool {
public:
    std::uint32_t allocate(NativeType native) {
        std::uint32_t id = next_id_++;
        map_[id] = native;
        return id;
    }

    NativeType lookup(std::uint32_t id) const {
        auto it = map_.find(id);
        return (it != map_.end()) ? it->second : NativeType{0};
    }

    void release(std::uint32_t id) {
        map_.erase(id);
    }

    void clear() { map_.clear(); next_id_ = 1; }

private:
    std::uint32_t next_id_ = 1;
    std::unordered_map<std::uint32_t, NativeType> map_;
};

// ---------------------------------------------------------------------------
// OpenGLRenderBackend
// ---------------------------------------------------------------------------

class OpenGLRenderBackend final : public RenderBackend {
public:
    ~OpenGLRenderBackend() override { shutdown(); }

    // --- Lifecycle ---
    bool initialize(void* native_window_handle) override {
        auto* glfw_window = static_cast<GLFWwindow*>(native_window_handle);
        if (!glfw_window) {
            log_error("RenderBackend: invalid native window handle");
            return false;
        }
        window_ = glfw_window;
        glfwMakeContextCurrent(window_);

#ifdef _WIN32
        // Load all GL function pointers via glad (Windows: opengl32.lib
        // only exports GL 1.1, modern functions need runtime resolution).
        gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress));
#endif

        return true;
    }

    void shutdown() override {
        if (window_) {
            glfwMakeContextCurrent(window_);
            buffers_.clear();
            shaders_.clear();
            queries_.clear();
            glfwMakeContextCurrent(nullptr);
            window_ = nullptr;
        }
    }

    void begin_frame() override {
        // GL context is already current; nothing needed here currently.
    }

    void end_frame() override {
        if (window_) {
            glfwSwapBuffers(window_);
        }
    }

    void get_framebuffer_size(int& width, int& height) override {
        if (window_) {
            glfwGetFramebufferSize(window_, &width, &height);
        } else {
            width = 1; height = 1;
        }
    }

    // --- Viewport & clear ---
    void set_viewport(int x, int y, int w, int h) override {
        glViewport(x, y, w, h);
    }

    void set_clear_color(float r, float g, float b, float a) override {
        glClearColor(r, g, b, a);
    }

    void clear_color_and_depth() override {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    // --- Depth state ---
    void set_depth_test(bool enabled) override {
        if (enabled) glEnable(GL_DEPTH_TEST);
        else         glDisable(GL_DEPTH_TEST);
    }

    void set_depth_func_less() override   { glDepthFunc(GL_LESS); }
    void set_depth_func_equal() override  { glDepthFunc(GL_EQUAL); }
    void set_depth_func_lequal() override { glDepthFunc(GL_LEQUAL); }

    void set_depth_write(bool enabled) override {
        glDepthMask(enabled ? GL_TRUE : GL_FALSE);
    }

    void set_color_write(bool r, bool g, bool b, bool a) override {
        glColorMask(r ? GL_TRUE : GL_FALSE,
                    g ? GL_TRUE : GL_FALSE,
                    b ? GL_TRUE : GL_FALSE,
                    a ? GL_TRUE : GL_FALSE);
    }

    // --- Buffers ---
    BufferHandle create_vertex_buffer(const void* data,
                                      std::size_t size_bytes,
                                      bool dynamic) override {
        GLuint vbo = 0;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(size_bytes),
                     data,
                     dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        BufferHandle h;
        h.id = buffers_.allocate(vbo);
        return h;
    }

    BufferHandle create_index_buffer(const std::uint32_t* data,
                                     std::size_t count) override {
        GLuint ibo = 0;
        glGenBuffers(1, &ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(count * sizeof(std::uint32_t)),
                     data,
                     GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        BufferHandle h;
        h.id = buffers_.allocate(ibo);
        return h;
    }

    void destroy_buffer(BufferHandle handle) override {
        if (!handle) return;
        GLuint vbo = buffers_.lookup(handle.id);
        if (vbo) glDeleteBuffers(1, &vbo);
        buffers_.release(handle.id);
    }

    void update_vertex_buffer(BufferHandle handle,
                              const void* data,
                              std::size_t size_bytes,
                              std::size_t offset) override {
        GLuint vbo = buffers_.lookup(handle.id);
        if (!vbo) return;
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER,
                        static_cast<GLintptr>(offset),
                        static_cast<GLsizeiptr>(size_bytes),
                        data);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    // --- Shaders ---
    ShaderHandle create_shader_program(const ShaderProgramDesc& desc) override {
        if (!desc.vertex_source || !desc.fragment_source) {
            log_error("RenderBackend: missing shader source");
            return kInvalidShader;
        }

        GLuint vs = compile_gl_shader(GL_VERTEX_SHADER, desc.vertex_source);
        if (!vs) return kInvalidShader;

        GLuint fs = compile_gl_shader(GL_FRAGMENT_SHADER, desc.fragment_source);
        if (!fs) {
            glDeleteShader(vs);
            return kInvalidShader;
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);

        for (int i = 0; i < desc.attrib_count; ++i) {
            glBindAttribLocation(program,
                                 static_cast<GLuint>(desc.attrib_locations[i]),
                                 desc.attrib_names[i]);
        }

        glLinkProgram(program);

        GLint success = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char info_log[512];
            glGetProgramInfoLog(program, 512, nullptr, info_log);
            log_error(std::string("GLSL program link failed: ") + info_log);
            glDeleteProgram(program);
            program = 0;
        }

        glDeleteShader(vs);
        glDeleteShader(fs);

        if (!program) return kInvalidShader;

        ShaderHandle h;
        h.id = shaders_.allocate(program);
        return h;
    }

    void destroy_shader(ShaderHandle handle) override {
        if (!handle) return;
        GLuint program = shaders_.lookup(handle.id);
        if (program) glDeleteProgram(program);
        shaders_.release(handle.id);
    }

    void use_shader(ShaderHandle handle) override {
        GLuint program = handle ? shaders_.lookup(handle.id) : 0;
        glUseProgram(program);
    }

    int get_uniform_location(ShaderHandle handle, const char* name) override {
        GLuint program = handle ? shaders_.lookup(handle.id) : 0;
        if (!program || !name) return -1;
        return glGetUniformLocation(program, name);
    }

    void set_uniform_int(int location, int value) override {
        if (location != -1) glUniform1i(location, value);
    }
    void set_uniform_float(int location, float value) override {
        if (location != -1) glUniform1f(location, value);
    }
    void set_uniform_vec2(int location, float x, float y) override {
        if (location != -1) glUniform2f(location, x, y);
    }
    void set_uniform_vec3(int location, float x, float y, float z) override {
        if (location != -1) glUniform3f(location, x, y, z);
    }
    void set_uniform_vec4(int location, float x, float y, float z, float w) override {
        if (location != -1) glUniform4f(location, x, y, z, w);
    }
    void set_uniform_mat4_array(int location,
                                const float* matrices,
                                int count) override {
        if (location != -1) {
            glUniformMatrix4fv(location, count, GL_FALSE, matrices);
        }
    }

    // --- Textures ---
    TextureHandle create_texture(int width, int height, const void* rgba8_data) override {
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba8_data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        return {textures_.allocate(tex)};
    }

    void destroy_texture(TextureHandle handle) override {
        GLuint tex = textures_.lookup(handle.id);
        if (tex != 0) {
            glDeleteTextures(1, &tex);
            textures_.release(handle.id);
        }
    }

    void bind_texture(TextureHandle handle, int slot) override {
        GLuint tex = textures_.lookup(handle.id);
        if (tex != 0) {
            glActiveTexture(GL_TEXTURE0 + slot);
            glBindTexture(GL_TEXTURE_2D, tex);
        }
    }

    // --- GPU mesh helpers ---
    GpuMesh create_gpu_mesh(const GltfMesh& mesh) override {
        GpuMesh gm;
        gm.color_r = mesh.color_r;
        gm.color_g = mesh.color_g;
        gm.color_b = mesh.color_b;
        gm.index_count  = static_cast<int>(mesh.indices.size());
        gm.vertex_count = static_cast<int>(mesh.positions.size() / 3);

        if (!mesh.positions.empty()) {
            gm.vbo_positions = create_vertex_buffer(
                mesh.positions.data(),
                mesh.positions.size() * sizeof(float), false);
        }
        if (!mesh.normals.empty()) {
            gm.vbo_normals = create_vertex_buffer(
                mesh.normals.data(),
                mesh.normals.size() * sizeof(float), false);
        }
        if (!mesh.uvs.empty()) {
            gm.vbo_texcoords = create_vertex_buffer(
                mesh.uvs.data(),
                mesh.uvs.size() * sizeof(float), false);
        }
        if (!mesh.joint_indices.empty()) {
            gm.vbo_joints = create_vertex_buffer(
                mesh.joint_indices.data(),
                mesh.joint_indices.size() * sizeof(float), false);
        }
        if (!mesh.joint_weights.empty()) {
            gm.vbo_weights = create_vertex_buffer(
                mesh.joint_weights.data(),
                mesh.joint_weights.size() * sizeof(float), false);
        }
        if (!mesh.indices.empty()) {
            gm.ibo_indices = create_index_buffer(
                mesh.indices.data(), mesh.indices.size());
        }
        return gm;
    }

    GpuModel create_gpu_model(const GltfModel& model) override {
        GpuModel gm;
        for (const auto& mesh : model.meshes) {
            gm.meshes.push_back(create_gpu_mesh(mesh));
        }
        return gm;
    }

    void destroy_gpu_mesh(GpuMesh& mesh) override {
        destroy_buffer(mesh.vbo_positions);
        destroy_buffer(mesh.vbo_normals);
        destroy_buffer(mesh.vbo_texcoords);
        destroy_buffer(mesh.vbo_joints);
        destroy_buffer(mesh.vbo_weights);
        destroy_buffer(mesh.ibo_indices);
        mesh = {};
    }

    void destroy_gpu_model(GpuModel& model) override {
        for (auto& m : model.meshes) {
            destroy_gpu_mesh(m);
        }
        model.meshes.clear();
    }

    // --- Draw calls ---
    void draw_gpu_mesh(const GpuMesh& mesh) override {
        if (mesh.vertex_count == 0) return;
        const GLuint vbo_pos = buffers_.lookup(mesh.vbo_positions.id);
        const GLuint ibo     = buffers_.lookup(mesh.ibo_indices.id);
        // Core-profile draw via the compat shader+VAO using the current matrix
        // state (flat-shaded; normals not used).
        ae::gl_compat::draw_user_arrays(vbo_pos, 0, 0, ibo, mesh.index_count,
                                        GL_TRIANGLES, 0, mesh.vertex_count);
    }

    void draw_gpu_mesh_skinned(const GpuMesh& mesh) override {
        // Flat-shaded via the compat path; skinning not yet ported, so static pose.
        if (mesh.vertex_count == 0) return;
        const GLuint vbo_pos = buffers_.lookup(mesh.vbo_positions.id);
        const GLuint ibo     = buffers_.lookup(mesh.ibo_indices.id);
        ae::gl_compat::draw_user_arrays(vbo_pos, 0, 0, ibo, mesh.index_count,
                                        GL_TRIANGLES, 0, mesh.vertex_count);
    }

    void draw_arrays_vnc(BufferHandle vbo_positions,
                         BufferHandle vbo_normals,
                         BufferHandle vbo_colors,
                         int first, int count,
                         int color_components = 3) override {
        (void)vbo_normals;  // flat-shaded; normals ignored
        const GLuint vbo_pos = buffers_.lookup(vbo_positions.id);
        const GLuint vbo_col = buffers_.lookup(vbo_colors.id);
        ae::gl_compat::draw_user_arrays(vbo_pos, vbo_col, color_components, 0, 0,
                                        GL_TRIANGLES, first, count);
    }

    void draw_arrays_positions(BufferHandle vbo_positions,
                               int first, int count) override {
        const GLuint vbo_pos = buffers_.lookup(vbo_positions.id);
        ae::gl_compat::draw_user_arrays(vbo_pos, 0, 0, 0, 0, GL_LINES, first, count);
    }

    // --- Queries ---
    QueryHandle create_query(bool is_timer) override {
        GLuint q = 0;
        glGenQueries(1, &q);
        QueryHandle h;
        h.id = queries_.allocate(q);
        return h;
    }

    void destroy_query(QueryHandle handle) override {
        if (!handle) return;
        GLuint q = queries_.lookup(handle.id);
        if (q) glDeleteQueries(1, &q);
        queries_.release(handle.id);
    }

    void begin_query(QueryHandle handle) override {
        GLuint q = queries_.lookup(handle.id);
        if (!q) return;
        glBeginQuery(GL_TIME_ELAPSED, q);
    }

    void end_query(QueryHandle handle) override {
        GLuint q = queries_.lookup(handle.id);
        if (!q) return;
        glEndQuery(GL_TIME_ELAPSED);
    }

    bool is_query_result_available(QueryHandle handle) override {
        GLuint q = queries_.lookup(handle.id);
        if (!q) return false;
        GLint available = 0;
        glGetQueryObjectiv(q, GL_QUERY_RESULT_AVAILABLE, &available);
        return available != 0;
    }

    std::uint32_t get_query_result_uint(QueryHandle handle) override {
        GLuint q = queries_.lookup(handle.id);
        if (!q) return 0;
        GLint result = 0;
        glGetQueryObjectiv(q, GL_QUERY_RESULT, &result);
        return static_cast<std::uint32_t>(result);
    }

    std::uint64_t get_query_result_uint64(QueryHandle handle) override {
        GLuint q = queries_.lookup(handle.id);
        if (!q) return 0;
        GLuint result = 0;
        glGetQueryObjectuiv(q, GL_QUERY_RESULT, &result);
        return static_cast<std::uint64_t>(result);
    }

    // --- Capability ---
    bool timer_queries_supported() override {
        const char* extensions =
            reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        if (!extensions) return false;
        std::string ext_str(extensions);
        return ext_str.find("GL_ARB_timer_query") != std::string::npos ||
               ext_str.find("GL_EXT_timer_query") != std::string::npos;
    }

    // --- Swap interval ---
    void set_swap_interval(int interval) override {
        if (window_) {
            glfwSwapInterval(interval);
        }
    }

private:
    static GLuint compile_gl_shader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char info_log[512];
            glGetShaderInfoLog(shader, 512, nullptr, info_log);
            log_error(std::string("GLSL compilation failed: ") + info_log);
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    GLFWwindow* window_ = nullptr;
    HandlePool<GLuint> buffers_;
    HandlePool<GLuint> shaders_;
    HandlePool<GLuint> queries_;
    HandlePool<GLuint> textures_;
};

}  // namespace

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<RenderBackend> create_opengl_backend() {
    return std::make_unique<OpenGLRenderBackend>();
}

}  // namespace ae::render
