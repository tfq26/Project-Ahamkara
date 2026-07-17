#include "ae/render/atmosphere_pass.h"
#include "ae/core/log.h"
#include "ae/render/gl_platform.h"

#include <cmath>
#include <cstring>

#define AE_LOG_CATEGORY "Render"

namespace ae::render {

AtmospherePass::AtmospherePass() = default;
AtmospherePass::~AtmospherePass() { shutdown(); }

bool AtmospherePass::initialize(RenderBackend* backend) {
    backend_ = backend;

    // Sky dome as a full-screen NDC quad (covers the entire screen)
    // The vertex shader will reverse-project to get world-space ray direction.
    const float quad[] = {-1,-1, 1,-1, -1,1, 1,1};
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // Procedural sky shader (simplified Nishita-style atmosphere)
    const char* vs = R"(
        #version 330 core
        layout(location=0) in vec2 aPos;
        out vec3 vRayDir;
        uniform mat4 uInvViewProj;
        void main() {
            vec4 clip = vec4(aPos, 1, 1);
            vec4 world = uInvViewProj * clip;
            vRayDir = world.xyz / world.w;
            gl_Position = vec4(aPos, 0, 1);
        }
    )";
    const char* fs = R"(
        #version 330 core
        in vec3 vRayDir;
        uniform vec3 uSunDir, uSunColor, uSkyColor, uHorizonColor;
        uniform float uSunIntensity;
        out vec4 fragColor;

        void main() {
            vec3 dir = normalize(vRayDir);
            float height = max(dir.y, 0.0);
            float horizon = 1.0 - abs(dir.y);

            // Sky gradient
            vec3 sky = mix(uHorizonColor, uSkyColor, pow(height, 0.5));
            sky = mix(sky, uHorizonColor * 0.7, pow(horizon, 1.5));

            // Sun
            float sun_dot = max(dot(dir, normalize(uSunDir)), 0.0);
            float sun_disk = pow(sun_dot, 100.0) * uSunIntensity;
            float sun_glow = pow(sun_dot, 8.0) * uSunIntensity * 0.3;
            vec3 sun = uSunColor * (sun_disk + sun_glow);

            fragColor = vec4(sky + sun, 1.0);
        }
    )";

    GLuint vs_id = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs_id, 1, &vs, nullptr);
    glCompileShader(vs_id);
    GLuint fs_id = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs_id, 1, &fs, nullptr);
    glCompileShader(fs_id);

    shader_ = glCreateProgram();
    glAttachShader(shader_, vs_id);
    glAttachShader(shader_, fs_id);
    glLinkProgram(shader_);

    GLint ok = 0;
    glGetProgramiv(shader_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetProgramInfoLog(shader_, 512, nullptr, log);
        ae::log_error(std::string("Atmosphere shader link: ") + log);
        return false;
    }
    glDeleteShader(vs_id);
    glDeleteShader(fs_id);

    u_view_proj_ = glGetUniformLocation(shader_, "uInvViewProj");
    u_sun_dir_ = glGetUniformLocation(shader_, "uSunDir");
    u_sun_color_ = glGetUniformLocation(shader_, "uSunColor");
    u_sun_intensity_ = glGetUniformLocation(shader_, "uSunIntensity");
    u_sky_color_ = glGetUniformLocation(shader_, "uSkyColor");
    u_horizon_color_ = glGetUniformLocation(shader_, "uHorizonColor");
    return true;
}

void AtmospherePass::shutdown() {
    if (shader_) glDeleteProgram(shader_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    shader_ = 0; vao_ = 0; vbo_ = 0;
}

void AtmospherePass::render(const float* view_matrix, const float* projection_matrix,
                            const TimeOfDay& tod) {
    if (!shader_ || !vao_) return;

    // Compute inverse of view*projection
    float vp[16];
    for (int i=0; i<4; ++i) for (int j=0; j<4; ++j)
        vp[i*4+j] = projection_matrix[i*4+0]*view_matrix[0*4+j]
                   + projection_matrix[i*4+1]*view_matrix[1*4+j]
                   + projection_matrix[i*4+2]*view_matrix[2*4+j]
                   + projection_matrix[i*4+3]*view_matrix[3*4+j];

    // Simple matrix inverse (assuming affine camera)
    float inv_vp[16];
    std::memset(inv_vp, 0, sizeof(inv_vp));
    for (int i=0; i<4; ++i) inv_vp[i*4+i] = 1;
    // Use Gauss-Jordan for the 4x4
    float m[16]; std::memcpy(m, vp, sizeof(vp));
    float r[16]; std::memcpy(r, inv_vp, sizeof(inv_vp));
    for (int col=0; col<4; ++col) {
        float pivot = m[col*4+col];
        if (std::abs(pivot) < 1e-10f) pivot = 1;
        for (int j=0; j<4; ++j) { m[j*4+col] /= pivot; r[j*4+col] /= pivot; }
        for (int row=0; row<4; ++row) {
            if (row == col) continue;
            float factor = m[col*4+row];
            for (int j=0; j<4; ++j) { m[j*4+row] -= factor * m[j*4+col]; r[j*4+row] -= factor * r[j*4+col]; }
        }
    }

    glUseProgram(shader_);
    glUniformMatrix4fv(u_view_proj_, 1, GL_FALSE, r);
    glUniform3f(u_sun_dir_, tod.sun_color[0], tod.sun_color[1], tod.sun_color[2]);
    glUniform3fv(u_sun_color_, 1, tod.sun_color);
    glUniform1f(u_sun_intensity_, tod.sun_intensity);
    glUniform3fv(u_sky_color_, 1, tod.sky_color);
    glUniform3fv(u_horizon_color_, 1, tod.horizon_color);

    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

} // namespace ae::render
