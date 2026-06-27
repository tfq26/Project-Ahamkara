#define AE_GL_COMPAT_NO_REMAP
#include "gl_compat.h"

#include <cstdio>
#include <cmath>

// We include the actual OpenGL headers for Core-compatible calls.
// All legacy calls are intercepted by the macros in gl_compat.h and routed
// through the functions in this file.
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#else
#include <GL/glcorearb.h>
#endif

namespace ae {
namespace gl_compat {

// ============================================================================
// Internal helpers
// ============================================================================

static GLuint s_compat_vao = 0;
static GLuint s_compat_vbo = 0;
static GLuint s_compat_shader = 0;
static GLint  s_u_mvp_loc = -1;
static GLint  s_u_color_loc = -1;
static int    s_viewport_w = 0;
static int    s_viewport_h = 0;

// --- GLSL 330 Core compat shader for batched immediate-mode primitives -------

static const char* kCompatVS =
    "#version 330 core\n"
    "layout(location = 0) in vec3 aPos;\n"
    "layout(location = 1) in vec4 aColor;\n"
    "uniform mat4 uMVP;\n"
    "out vec4 vColor;\n"
    "void main() {\n"
    "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
    "    vColor = aColor;\n"
    "}\n";

static const char* kCompatFS =
    "#version 330 core\n"
    "uniform vec4 uColor;\n"
    "in vec4 vColor;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    fragColor = vColor * uColor;\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        fprintf(stderr, "GL Compat shader compile failed:\n%s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

// ============================================================================
// Mat4
// ============================================================================

Mat4 Mat4::identity() { return Mat4(); }

Mat4 Mat4::operator*(const Mat4& b) const {
    Mat4 r;
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row) {
            float sum = 0;
            for (int k = 0; k < 4; ++k)
                sum += m[k * 4 + row] * b.m[col * 4 + k];
            r.m[col * 4 + row] = sum;
        }
    return r;
}

Mat4 mat4_translate(float x, float y, float z) {
    Mat4 r;
    r.m[12] = x; r.m[13] = y; r.m[14] = z;
    return r;
}

Mat4 mat4_rotate(float degrees, float x, float y, float z) {
    float rad = degrees * (3.1415926535f / 180.0f);
    float c = std::cos(rad), s = std::sin(rad);
    float len = std::sqrt(x*x + y*y + z*z);
    if (len < 1e-6f) return Mat4();
    x /= len; y /= len; z /= len;
    float nc = 1.0f - c;
    Mat4 r;
    r.m[0] = x*x*nc + c;    r.m[4] = x*y*nc - z*s;  r.m[8]  = x*z*nc + y*s;
    r.m[1] = y*x*nc + z*s;  r.m[5] = y*y*nc + c;    r.m[9]  = y*z*nc - x*s;
    r.m[2] = z*x*nc - y*s;  r.m[6] = z*y*nc + x*s;  r.m[10] = z*z*nc + c;
    return r;
}

Mat4 mat4_scale(float x, float y, float z) {
    Mat4 r;
    r.m[0] = x; r.m[5] = y; r.m[10] = z;
    return r;
}

Mat4 mat4_perspective(float fov_rad, float aspect, float near, float far) {
    Mat4 r;
    float f = 1.0f / std::tan(fov_rad * 0.5f);
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (far + near) / (near - far);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * far * near) / (near - far);
    r.m[15] = 0.0f;
    return r;
}

Mat4 mat4_ortho(float left, float right, float bottom, float top, float near, float far) {
    Mat4 r;
    r.m[0] = 2.0f / (right - left);
    r.m[5] = 2.0f / (top - bottom);
    r.m[10] = -2.0f / (far - near);
    r.m[12] = -(right + left) / (right - left);
    r.m[13] = -(top + bottom) / (top - bottom);
    r.m[14] = -(far + near) / (far - near);
    return r;
}

// ============================================================================
// State
// ============================================================================

GLCompatState& state() {
    static GLCompatState s;
    return s;
}

// --- Diagnostics ------------------------------------------------------------
static int s_diag_verts = 0;
static int s_diag_draws = 0;
bool ready() { return s_compat_shader != 0; }
void diag_reset() { s_diag_verts = 0; s_diag_draws = 0; }
int diag_vertices() { return s_diag_verts; }
int diag_draws() { return s_diag_draws; }

void draw_user_arrays(unsigned int vbo_pos, unsigned int vbo_col, int color_components,
                      unsigned int ibo, int index_count,
                      unsigned int mode, int first, int count) {
    if (s_compat_shader == 0 || s_compat_vao == 0 || vbo_pos == 0) return;
    auto& st = state();

    glUseProgram(s_compat_shader);
    Mat4 mvp = st.projection * st.modelview;
    glUniformMatrix4fv(s_u_mvp_loc, 1, GL_FALSE, mvp.m);

    glBindVertexArray(s_compat_vao);

    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(vbo_pos));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    if (vbo_col != 0 && color_components > 0) {
        if (s_u_color_loc != -1) glUniform4f(s_u_color_loc, 1.0f, 1.0f, 1.0f, 1.0f);
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(vbo_col));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, color_components, GL_FLOAT, GL_FALSE, 0, (void*)0);
    } else {
        if (s_u_color_loc != -1)
            glUniform4f(s_u_color_loc, st.current_r, st.current_g, st.current_b, st.current_a);
        glDisableVertexAttribArray(1);
        glVertexAttrib4f(1, 1.0f, 1.0f, 1.0f, 1.0f);
    }

    int drawn = 0;
    if (ibo != 0 && index_count > 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(ibo));
        glDrawElements(static_cast<GLenum>(mode), index_count, GL_UNSIGNED_INT, (void*)0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        drawn = index_count;
    } else if (count > 0) {
        glDrawArrays(static_cast<GLenum>(mode), first, count);
        drawn = count;
    }
    s_diag_verts += drawn;
    ++s_diag_draws;

    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}

// ============================================================================
// Flush batch
// ============================================================================

// Convert quads to triangles: each 4-vertex quad → 6 vertices (2 triangles)
// Pattern: [v0,v1,v2] [v0,v2,v3]
static void quad_to_triangles(std::vector<VertexP3C4N3>& verts) {
    if (verts.size() < 4) return;
    size_t quad_count = verts.size() / 4;
    std::vector<VertexP3C4N3> tris;
    tris.reserve(quad_count * 6);
    for (size_t q = 0; q < quad_count; ++q) {
        size_t base = q * 4;
        // Triangle 1: v0, v1, v2
        tris.push_back(verts[base + 0]);
        tris.push_back(verts[base + 1]);
        tris.push_back(verts[base + 2]);
        // Triangle 2: v0, v2, v3
        tris.push_back(verts[base + 0]);
        tris.push_back(verts[base + 2]);
        tris.push_back(verts[base + 3]);
    }
    verts = std::move(tris);
}

static void quad_to_triangles_2d(std::vector<VertexP2C4>& verts) {
    if (verts.size() < 4) return;
    size_t quad_count = verts.size() / 4;
    std::vector<VertexP2C4> tris;
    tris.reserve(quad_count * 6);
    for (size_t q = 0; q < quad_count; ++q) {
        size_t base = q * 4;
        tris.push_back(verts[base + 0]);
        tris.push_back(verts[base + 1]);
        tris.push_back(verts[base + 2]);
        tris.push_back(verts[base + 0]);
        tris.push_back(verts[base + 2]);
        tris.push_back(verts[base + 3]);
    }
    verts = std::move(tris);
}

static void flush_batch_3d() {
    auto& st = state();
    if (st.batch_vertices.empty()) return;

    // Convert quads to triangles (GL_QUADS → GL_TRIANGLES)
    if (st.begin_mode == 0x0007) {
        quad_to_triangles(st.batch_vertices);
    }

    glUseProgram(s_compat_shader);

    // Compute MVP
    Mat4 mvp = st.projection * st.modelview;

    glUniformMatrix4fv(s_u_mvp_loc, 1, GL_FALSE, mvp.m);
    if (s_u_color_loc != -1)
        glUniform4f(s_u_color_loc, 1.0f, 1.0f, 1.0f, 1.0f);

    glBindVertexArray(s_compat_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_compat_vbo);
    glBufferData(GL_ARRAY_BUFFER,
        (GLsizeiptr)(st.batch_vertices.size() * sizeof(VertexP3C4N3)),
        st.batch_vertices.data(), GL_STREAM_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexP3C4N3), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(VertexP3C4N3),
        (void*)(sizeof(float)*3));
    glEnableVertexAttribArray(1);
    glDisableVertexAttribArray(2);

    GLenum mode = GL_TRIANGLES;
    switch (st.begin_mode) {
        case 0x0000: mode = GL_POINTS; break;
        case 0x0001: mode = GL_LINES; break;
        case 0x0002: mode = GL_LINE_LOOP; break;
        case 0x0003: mode = GL_LINE_STRIP; break;
        case 0x0004: mode = GL_TRIANGLES; break;
        case 0x0006: mode = GL_TRIANGLE_FAN; break;
        case 0x0007: mode = GL_TRIANGLES; break;  // quads are triangulated below
        case 0x0008: mode = GL_TRIANGLE_STRIP; break;
        default: break;
    }

    s_diag_verts += static_cast<int>(st.batch_vertices.size());
    ++s_diag_draws;
    glDrawArrays(mode, 0, (GLsizei)st.batch_vertices.size());

    glBindVertexArray(0);
    glUseProgram(0);
    st.batch_vertices.clear();
}

// Note: in gl_compat.h, begin_mode stores the raw GL enum value.
// For GL_QUADS (0x0007), each quad has 4 vertices. We triangulate:
// vertices 0,1,2 then 0,2,3 for each group of 4.
// This happens in aecBegin / aecVertex3f / aecEnd.

static void flush_batch_2d() {
    auto& st = state();
    if (st.batch_vertices_2d.empty()) return;

    if (st.begin_mode == 0x0007) {
        quad_to_triangles_2d(st.batch_vertices_2d);
    }

    // For 2D, use orthographic projection spanning the full viewport
    glUseProgram(s_compat_shader);

    Mat4 ortho;
    ortho.m[0] = 2.0f / s_viewport_w;
    ortho.m[5] = -2.0f / s_viewport_h;
    ortho.m[10] = -1.0f;
    ortho.m[12] = -1.0f;
    ortho.m[13] = 1.0f;
    ortho.m[14] = 0.0f;

    glUniformMatrix4fv(s_u_mvp_loc, 1, GL_FALSE, ortho.m);
    if (s_u_color_loc != -1)
        glUniform4f(s_u_color_loc, 1.0f, 1.0f, 1.0f, 1.0f);

    glBindVertexArray(s_compat_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_compat_vbo);
    glBufferData(GL_ARRAY_BUFFER,
        (GLsizeiptr)(st.batch_vertices_2d.size() * sizeof(VertexP2C4)),
        st.batch_vertices_2d.data(), GL_STREAM_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(VertexP2C4), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(VertexP2C4),
        (void*)(sizeof(float)*2));
    glEnableVertexAttribArray(1);
    glDisableVertexAttribArray(2);

    GLenum mode = GL_TRIANGLES;
    switch (st.begin_mode) {
        case 0x0000: mode = GL_POINTS; break;
        case 0x0001: mode = GL_LINES; break;
        case 0x0002: mode = GL_LINE_LOOP; break;
        case 0x0003: mode = GL_LINE_STRIP; break;
        case 0x0004: mode = GL_TRIANGLES; break;
        case 0x0006: mode = GL_TRIANGLE_FAN; break;
        case 0x0007: mode = GL_TRIANGLES; break;
        case 0x0008: mode = GL_TRIANGLE_STRIP; break;
        default: break;
    }

    s_diag_verts += static_cast<int>(st.batch_vertices_2d.size());
    ++s_diag_draws;
    glDrawArrays(mode, 0, (GLsizei)st.batch_vertices_2d.size());

    glBindVertexArray(0);
    glUseProgram(0);
    st.batch_vertices_2d.clear();
}

// ============================================================================
// Init / shutdown
// ============================================================================

void init() {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, kCompatVS);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, kCompatFS);
    if (!vs || !fs) return;

    s_compat_shader = glCreateProgram();
    glAttachShader(s_compat_shader, vs);
    glAttachShader(s_compat_shader, fs);
    glBindAttribLocation(s_compat_shader, 0, "aPos");
    glBindAttribLocation(s_compat_shader, 1, "aColor");
    glLinkProgram(s_compat_shader);
    glDeleteShader(vs);
    glDeleteShader(fs);

    s_u_mvp_loc   = glGetUniformLocation(s_compat_shader, "uMVP");
    s_u_color_loc = glGetUniformLocation(s_compat_shader, "uColor");

    glGenVertexArrays(1, &s_compat_vao);
    glGenBuffers(1, &s_compat_vbo);
}

void shutdown() {
    glDeleteProgram(s_compat_shader);
    glDeleteVertexArrays(1, &s_compat_vao);
    glDeleteBuffers(1, &s_compat_vbo);
    s_compat_shader = 0;
    s_compat_vao = 0;
    s_compat_vbo = 0;
}

void begin_frame(int vp_w, int vp_h) {
    s_viewport_w = vp_w;
    s_viewport_h = vp_h;
}

// ============================================================================
// Immediate mode (batch vertices, flush at glEnd)
// ============================================================================

void aecBegin(int mode) {
    auto& st = state();
    st.begin_mode = mode;
    st.batch_vertices.clear();
    st.batch_vertices_2d.clear();
}

void aecEnd() {
    auto& st = state();
    // If we have 3D vertices, use them; otherwise try 2D
    if (!st.batch_vertices.empty())
        flush_batch_3d();
    else if (!st.batch_vertices_2d.empty())
        flush_batch_2d();
    st.begin_mode = 0;
}

void aecColor3f(float r, float g, float b) {
    state().current_r = r; state().current_g = g; state().current_b = b; state().current_a = 1.0f;
}

void aecColor4f(float r, float g, float b, float a) {
    state().current_r = r; state().current_g = g; state().current_b = b; state().current_a = a;
}

void aecColor3ub(unsigned char r, unsigned char g, unsigned char b) {
    state().current_r = r/255.0f; state().current_g = g/255.0f; state().current_b = b/255.0f; state().current_a = 1.0f;
}

void aecVertex3f(float x, float y, float z) {
    auto& st = state();
    VertexP3C4N3 v;
    v.x = x; v.y = y; v.z = z;
    v.r = st.current_r; v.g = st.current_g; v.b = st.current_b; v.a = st.current_a;
    v.nx = st.current_nx; v.ny = st.current_ny; v.nz = st.current_nz;
    st.batch_vertices.push_back(v);
}

void aecVertex2f(float x, float y) {
    auto& st = state();
    VertexP2C4 v;
    v.x = x; v.y = y;
    v.r = st.current_r; v.g = st.current_g; v.b = st.current_b; v.a = st.current_a;
    st.batch_vertices_2d.push_back(v);
}

void aecNormal3f(float nx, float ny, float nz) {
    state().current_nx = nx; state().current_ny = ny; state().current_nz = nz;
}

void aecTexCoord2f(float u, float v) {
    state().current_u = u; state().current_v = v;
}

// ============================================================================
// Lighting / enable/disable dispatch
// ============================================================================

// Caps that exist in Core Profile and must be passed through to real glEnable
static bool is_core_cap(int cap) {
    switch (cap) {
        case 0x0B71:  // GL_DEPTH_TEST
        case 0x0BE2:  // GL_BLEND
        case 0x0BC0:  // GL_ALPHA_TEST (actually removed in 3.3, but safe)
        case 0x0B44:  // GL_CULL_FACE
        case 0x0DE1:  // GL_TEXTURE_2D (removed in 3.3)
        case 0x809D:  // GL_MULTISAMPLE
        case 0x8642:  // GL_PROGRAM_POINT_SIZE
        case 0x0B60:  // GL_FOG
        case 0x0C11:  // GL_SCISSOR_TEST
        case 0x0BD0:  // GL_DITHER
        case 0x0B90:  // GL_STENCIL_TEST
            return true;
        default:
            return false;
    }
}

void aecEnable(int cap) {
    if (is_core_cap(cap)) {
        glEnable(cap);
    }
    // Legacy caps (GL_LIGHTING, GL_LIGHT0, GL_COLOR_MATERIAL, etc.) are no-ops:
    // lighting is now handled per-shader with custom uniforms.
}

void aecDisable(int cap) {
    if (is_core_cap(cap)) {
        glDisable(cap);
    }
}

void aecLightfv(int,int,const float*) {}
void aecLightModelfv(int,const float*) {}
void aecLightf(int,int,float) {}
void aecLighti(int,int,int) {}
void aecColorMaterial(int,int) {}
void aecShadeModel(int) {}

void aecEnableClientState(int) {}
void aecDisableClientState(int) {}
void aecVertexPointer(int,int,int,const void*) {}
void aecNormalPointer(int,int,const void*) {}
void aecColorPointer(int,int,int,const void*) {}

}  // namespace gl_compat
}  // namespace ae
