#pragma once

// GL Compatibility Shim — bridges legacy-style fixed-function coding patterns
// to OpenGL 3.3 Core.  Include this header in any file that uses the compat
// layer; the macro remaps below intercept legacy GL calls and route them
// through safe core-profile equivalents.
//
// What remains (necessary for the debug renderer):
//   - Mat4 column-major matrix type + math helpers
//   - GLCompatState singleton (current color, projection/modelview matrices)
//   - draw_user_arrays() — renders user VBOs through the compat shader
//   - aecEnable/aecDisable — passes core caps through to real GL
//   - aecColor3f/4f/3ub — stores current color in state
//   - Macro remaps for glEnable/glDisable, glColor3f/4f/3ub
//
// What was removed (dead code, zero consumers):
//   - Immediate-mode batch system (aecBegin/aecEnd/aecVertex/aecNormal/etc.)
//   - Lighting stubs (aecLight*/aecColorMaterial/aecShadeModel)
//   - Client-state stubs (aecEnableClientState/aecVertexPointer/etc.)
//   - Unused vertex types (VertexP3, VertexP2)
//   - Unused legacy enum constants

#include <cstring>
#include <vector>

namespace ae {
namespace gl_compat {

// --- Vertex types ------------------------------------------------------------

struct VertexP3C4 {
    float x, y, z;
    float r, g, b, a;
};

struct VertexP3C4N3 {
    float x, y, z;
    float r, g, b, a;
    float nx, ny, nz;
};

struct VertexP2C4 {
    float x, y;
    float r, g, b, a;
};

// --- Matrix type (column-major, 4x4) -----------------------------------------

struct Mat4 {
    float m[16];
    Mat4() { std::memset(m, 0, sizeof(m)); m[0]=m[5]=m[10]=m[15]=1.0f; }
    static Mat4 identity();
    Mat4 operator*(const Mat4& other) const;
};

Mat4 mat4_translate(float x, float y, float z);
Mat4 mat4_rotate(float degrees, float x, float y, float z);
Mat4 mat4_scale(float x, float y, float z);
Mat4 mat4_perspective(float fov_rad, float aspect, float near, float far);
Mat4 mat4_ortho(float left, float right, float bottom, float top, float near, float far);

// --- Context (per-thread singleton) ------------------------------------------

struct GLCompatState {
    Mat4 modelview;
    Mat4 projection;

    // Current color — set via aecColor3f/4f/3ub and read by consumer code
    // when assembling vertex structs.
    float current_r {1.0f}, current_g {1.0f}, current_b {1.0f}, current_a {1.0f};
};

GLCompatState& state();

// --- Initialization / frame management ---------------------------------------

void init();
void shutdown();
void begin_frame(int viewport_w, int viewport_h);

// Diagnostics: was the compat shader built, and how much geometry was flushed.
bool ready();
void diag_reset();
int diag_vertices();
int diag_draws();

// Core-profile replacement for legacy client-array draws: render user VBOs
// (position at attrib 0, optional color at attrib 1) through the compat
// shader + VAO using the current matrix state. Flat-shaded (normals ignored).
// If vbo_col == 0, the current gl_compat color is used as a flat uniform color.
// If ibo != 0, draws indexed (index_count GL_UNSIGNED_INT); else glDrawArrays.
void draw_user_arrays(unsigned int vbo_pos, unsigned int vbo_col, int color_components,
                      unsigned int ibo, int index_count,
                      unsigned int mode, int first, int count);

// --- Public API (called via remapped GL names) -------------------------------

void aecColor3f(float r, float g, float b);
void aecColor4f(float r, float g, float b, float a);
void aecColor3ub(unsigned char r, unsigned char g, unsigned char b);

void aecEnable(int cap);
void aecDisable(int cap);

}  // namespace gl_compat
}  // namespace ae

// --- Macro remapping ---------------------------------------------------------

#ifndef AE_GL_COMPAT_NO_REMAP

// Primitive types (used by consumer code for draw modes)
#define GL_POINTS         0x0000
#define GL_LINES          0x0001
#define GL_LINE_LOOP      0x0002
#define GL_LINE_STRIP     0x0003
#define GL_TRIANGLES      0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN   0x0006

// glEnable/glDisable: core caps (GL_DEPTH_TEST, GL_BLEND, etc.) are passed
// through to real GL; legacy caps are no-ops.
#define glEnable(cap)           ae::gl_compat::aecEnable(cap)
#define glDisable(cap)          ae::gl_compat::aecDisable(cap)

// Color: stores current color in GLCompatState for use by vertex assembly code.
#define glColor3f(r,g,b)     ae::gl_compat::aecColor3f(r,g,b)
#define glColor4f(r,g,b,a)   ae::gl_compat::aecColor4f(r,g,b,a)
#define glColor3ub(r,g,b)    ae::gl_compat::aecColor3ub(r,g,b)

#endif // AE_GL_COMPAT_NO_REMAP
