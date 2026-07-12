#define AE_GL_COMPAT_NO_REMAP
#include "gl_compat.h"
#include "ae/core/log.h"

#include <cstdio>
#include <cmath>

#define AE_LOG_CATEGORY "Render"

// We include the actual OpenGL headers for Core-compatible calls.
// All legacy calls are intercepted by the macros in gl_compat.h and routed
// through the functions in this file.
#include "ae/render/gl_platform.h"

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

// --- GLSL 330 Core compat shader  -------------------------------------------

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
        ae::log_error_cat(AE_LOG_CATEGORY, std::string("GL Compat shader compile failed: ") + log);
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
// Init / shutdown
// ============================================================================

void init() {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, kCompatVS);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, kCompatFS);
    if (!vs || !fs) {
        ae::log_error_cat(AE_LOG_CATEGORY, "GL Compat layer: shader compilation failed");
        return;
    }

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
    ae::log_info_cat(AE_LOG_CATEGORY, "GL Compat layer initialized");
}

void shutdown() {
    glDeleteProgram(s_compat_shader);
    glDeleteVertexArrays(1, &s_compat_vao);
    glDeleteBuffers(1, &s_compat_vbo);
    s_compat_shader = 0;
    s_compat_vao = 0;
    s_compat_vbo = 0;
    ae::log_debug_cat(AE_LOG_CATEGORY, "GL Compat layer shut down");
}

void begin_frame(int vp_w, int vp_h) {
    s_viewport_w = vp_w;
    s_viewport_h = vp_h;
}

// ============================================================================
// Color state
// ============================================================================

void aecColor3f(float r, float g, float b) {
    state().current_r = r; state().current_g = g; state().current_b = b; state().current_a = 1.0f;
}

void aecColor4f(float r, float g, float b, float a) {
    state().current_r = r; state().current_g = g; state().current_b = b; state().current_a = a;
}

void aecColor3ub(unsigned char r, unsigned char g, unsigned char b) {
    state().current_r = r/255.0f; state().current_g = g/255.0f; state().current_b = b/255.0f; state().current_a = 1.0f;
}

// ============================================================================
// Enable / disable dispatch
// ============================================================================

// Caps that exist in Core Profile and must be passed through to real glEnable.
// Caps removed in 3.3 (GL_ALPHA_TEST, GL_TEXTURE_2D, GL_FOG) are included
// for safety — they are silently passed to the driver which may ignore them
// in a strict core context, but including them avoids breaking old caller code.
static bool is_core_cap(int cap) {
    switch (cap) {
        case 0x0B71:  // GL_DEPTH_TEST
        case 0x0BE2:  // GL_BLEND
        case 0x0BC0:  // GL_ALPHA_TEST (removed in 3.3, safe passthrough)
        case 0x0B44:  // GL_CULL_FACE
        case 0x0DE1:  // GL_TEXTURE_2D (removed in 3.3)
        case 0x809D:  // GL_MULTISAMPLE
        case 0x8642:  // GL_PROGRAM_POINT_SIZE
        case 0x0B60:  // GL_FOG (removed in 3.3)
        case 0x0C11:  // GL_SCISSOR_TEST
        case 0x0BD0:  // GL_DITHER
        case 0x0B90:  // GL_STENCIL_TEST
        case 0x0B7D:  // GL_POLYGON_OFFSET_FILL (core in 3.3, used by decal pass)
            return true;
        default:
            return false;
    }
}

void aecEnable(int cap) {
    if (is_core_cap(cap)) {
        glEnable(cap);
    }
}

void aecDisable(int cap) {
    if (is_core_cap(cap)) {
        glDisable(cap);
    }
}

}  // namespace gl_compat
}  // namespace ae
