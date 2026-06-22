#pragma once

// ============================================================================
// GL Compatibility Shim — remaps OpenGL 2.1 fixed-function calls to 3.3 Core
// equivalents.  Include this header in any file that uses legacy GL APIs then
// #undef the remaps at the end of the file to avoid leaking into others.
// ============================================================================

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

struct VertexP3 {
    float x, y, z;
};

struct VertexP2 {
    float x, y;
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

// --- Context (per-thread singleton) ------------------------------------------

struct GLCompatState {
    // Current matrix mode
    enum { kModelView, kProjection } matrix_mode {kModelView};
    Mat4 modelview;
    Mat4 projection;
    Mat4 modelview_stack[16];
    int modelview_stack_depth {0};
    Mat4 projection_stack[16];
    int projection_stack_depth {0};

    // Current color
    float current_r {1.0f}, current_g {1.0f}, current_b {1.0f}, current_a {1.0f};
    
    // Current normal
    float current_nx {0}, current_ny {0}, current_nz {1};
    
    // Current texcoord
    float current_u {0}, current_v {0};

    // Batch state
    int begin_mode {0};  // 0 = not inside glBegin/glEnd
    std::vector<VertexP3C4N3> batch_vertices;
    std::vector<VertexP2C4> batch_vertices_2d;

    // Lighting emulation state
    bool lighting_enabled {false};
    bool color_material_enabled {false};
    float light_model_ambient[4] {0.2f, 0.2f, 0.2f, 1.0f};
    float light0_position[4] {1.0f, 5.0f, 2.0f, 0.0f};
    float light0_diffuse[4] {0.9f, 0.9f, 0.85f, 1.0f};
    float light0_specular[4] {0.5f, 0.5f, 0.5f, 1.0f};
    float light1_position[4] {-1.0f, 10.0f, -2.0f, 0.0f};
    float light1_diffuse[4] {0.4f, 0.45f, 0.55f, 1.0f};
    float light1_specular[4] {0.2f, 0.2f, 0.25f, 1.0f};
};

GLCompatState& state();

// --- Initialization / frame management ---------------------------------------

void init();
void shutdown();
void begin_frame(int viewport_w, int viewport_h);

// --- Public API (called via remapped GL names) -------------------------------

void aecMatrixMode(int mode);
void aecLoadIdentity();
void aecLoadMatrixf(const float* m);
void aecPushMatrix();
void aecPopMatrix();
void aecTranslatef(float x, float y, float z);
void aecRotatef(float angle, float x, float y, float z);
void aecScalef(float x, float y, float z);
void aecGetFloatv(int pname, float* params);

void aecBegin(int mode);
void aecEnd();

void aecColor3f(float r, float g, float b);
void aecColor4f(float r, float g, float b, float a);
void aecColor3ub(unsigned char r, unsigned char g, unsigned char b);
void aecVertex3f(float x, float y, float z);
void aecVertex2f(float x, float y);
void aecNormal3f(float nx, float ny, float nz);
void aecTexCoord2f(float u, float v);

void aecEnable(int cap);
void aecDisable(int cap);
void aecLightfv(int light, int pname, const float* params);
void aecLightModelfv(int pname, const float* params);
void aecLightf(int light, int pname, float param);
void aecLighti(int light, int pname, int param);
void aecColorMaterial(int face, int mode);
void aecShadeModel(int mode);

void aecEnableClientState(int cap);
void aecDisableClientState(int cap);
void aecVertexPointer(int size, int type, int stride, const void* ptr);
void aecNormalPointer(int type, int stride, const void* ptr);
void aecColorPointer(int size, int type, int stride, const void* ptr);

}  // namespace gl_compat
}  // namespace ae

// --- Macro remapping (include this file to replace all legacy GL calls) -------

#ifndef AE_GL_COMPAT_NO_REMAP

#define GL_MODELVIEW  0x1700
#define GL_PROJECTION 0x1701
#define GL_MODELVIEW_MATRIX  0x0BA6
#define GL_PROJECTION_MATRIX 0x0BA7

#define GL_POINTS         0x0000
#define GL_LINES          0x0001
#define GL_LINE_LOOP      0x0002
#define GL_LINE_STRIP     0x0003
#define GL_TRIANGLES      0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN   0x0006
#define GL_QUADS          0x0007
#define GL_QUAD_STRIP     0x0008

#define GL_LIGHTING       0x0B50
#define GL_LIGHT0         0x4000
#define GL_LIGHT1         0x4001
#define GL_COLOR_MATERIAL 0x0B57
#define GL_AMBIENT        0x1200
#define GL_DIFFUSE        0x1201
#define GL_SPECULAR       0x1202
#define GL_POSITION       0x1203
#define GL_AMBIENT_AND_DIFFUSE 0x1602
#define GL_LIGHT_MODEL_AMBIENT 0x0B53
#define GL_FRONT_AND_BACK      0x0408
#define GL_FRONT         0x0404
#define GL_SHADE_MODEL   0x0B54
#define GL_SMOOTH        0x1D01

#define GL_VERTEX_ARRAY          0x8074
#define GL_NORMAL_ARRAY          0x8075
#define GL_COLOR_ARRAY           0x8076
#define GL_FLOAT                 0x1406

// glEnable/glDisable: core caps (GL_DEPTH_TEST, GL_BLEND, etc.) are passed
// through to real GL; legacy caps (GL_LIGHTING, GL_LIGHT0, GL_COLOR_MATERIAL,
// etc.) are no-ops since lighting is handled per-shader now.
#define glEnable(cap)           ae::gl_compat::aecEnable(cap)
#define glDisable(cap)          ae::gl_compat::aecDisable(cap)
#define glLightfv(light,pname,p) ae::gl_compat::aecLightfv(light,pname,p)
#define glLoadIdentity()    ae::gl_compat::aecLoadIdentity()
#define glLoadMatrixf(m)    ae::gl_compat::aecLoadMatrixf(m)
#define glPushMatrix()      ae::gl_compat::aecPushMatrix()
#define glPopMatrix()       ae::gl_compat::aecPopMatrix()
#define glTranslatef(x,y,z) ae::gl_compat::aecTranslatef(x,y,z)
#define glRotatef(a,x,y,z)  ae::gl_compat::aecRotatef(a,x,y,z)
#define glScalef(x,y,z)     ae::gl_compat::aecScalef(x,y,z)
#define glGetFloatv(p,params) ae::gl_compat::aecGetFloatv(p,params)

#define glBegin(m)    ae::gl_compat::aecBegin(m)
#define glEnd()       ae::gl_compat::aecEnd()

#define glColor3f(r,g,b)     ae::gl_compat::aecColor3f(r,g,b)
#define glColor4f(r,g,b,a)   ae::gl_compat::aecColor4f(r,g,b,a)
#define glColor3ub(r,g,b)    ae::gl_compat::aecColor3ub(r,g,b)
#define glVertex3f(x,y,z)    ae::gl_compat::aecVertex3f(x,y,z)
#define glVertex2f(x,y)      ae::gl_compat::aecVertex2f(x,y)
#define glNormal3f(nx,ny,nz) ae::gl_compat::aecNormal3f(nx,ny,nz)
#define glTexCoord2f(u,v)    ae::gl_compat::aecTexCoord2f(u,v)

// Lighting + client state: use aec wrappers for the compat-only ones
#define glLightfv(light,pname,p) ae::gl_compat::aecLightfv(light,pname,p)
#define glLightModelfv(pname,p)  ae::gl_compat::aecLightModelfv(pname,p)
#define glLightf(light,pname,p)  ae::gl_compat::aecLightf(light,pname,p)
#define glLighti(light,pname,p)  ae::gl_compat::aecLighti(light,pname,p)
#define glColorMaterial(face,mode) ae::gl_compat::aecColorMaterial(face,mode)
#define glShadeModel(mode)       ae::gl_compat::aecShadeModel(mode)
#define glEnableClientState(cap)     ae::gl_compat::aecEnableClientState(cap)
#define glDisableClientState(cap)    ae::gl_compat::aecDisableClientState(cap)
#define glVertexPointer(s,t,stride,p) ae::gl_compat::aecVertexPointer(s,t,stride,p)
#define glNormalPointer(t,stride,p)   ae::gl_compat::aecNormalPointer(t,stride,p)
#define glColorPointer(s,t,stride,p)  ae::gl_compat::aecColorPointer(s,t,stride,p)

#endif // AE_GL_COMPAT_NO_REMAP
