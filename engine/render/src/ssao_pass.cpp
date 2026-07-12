#include "ae/render/ssao_pass.h"
#include "ae/core/log.h"
#include "ae/render/gl_platform.h"

#include <cmath>
#include <cstring>

#define AE_LOG_CATEGORY "Render"

namespace ae::render {

SsaoPass::SsaoPass() = default;
SsaoPass::~SsaoPass() { shutdown(); }

bool SsaoPass::initialize(RenderBackend* backend, int width, int height) {
    backend_ = backend;

    // AO render target
    GLuint ao_gl = 0;
    glGenTextures(1, &ao_gl);
    glBindTexture(GL_TEXTURE_2D, ao_gl);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    ao_tex_.id = ao_gl;

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ao_gl, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Full-screen quad VAO
    const float quad[] = {
        -1, -1, 0, 0,   1, -1, 1, 0,
        -1,  1, 0, 1,   1,  1, 1, 1,
    };
    glGenVertexArrays(1, &quad_vao_);
    glGenBuffers(1, &quad_vbo_);
    glBindVertexArray(quad_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // SSAO shader
    const char* vs = R"(
        #version 330 core
        layout(location=0) in vec2 aPos;
        layout(location=1) in vec2 aUV;
        out vec2 vUV;
        void main() { gl_Position = vec4(aPos,0,1); vUV = aUV; }
    )";
    const char* fs = R"(
        #version 330 core
        in vec2 vUV;
        uniform sampler2D uDepthTex;
        uniform mat4 uProj;
        uniform float uRadius, uPower, uBias;
        out float fragAO;
        const float PI = 3.14159265359;

        // Reconstruct view-space position from depth
        vec3 view_pos(vec2 uv, float d) {
            vec4 clip = vec4(uv*2.0-1.0, d*2.0-1.0, 1.0);
            vec4 v = inverse(uProj) * clip;
            return v.xyz / v.w;
        }

        void main() {
            float d = texture(uDepthTex, vUV).r;
            if (d >= 1.0) { fragAO = 1.0; return; }
            vec3 pos = view_pos(vUV, d);
            vec3 normal = normalize(cross(dFdx(pos), dFdy(pos)));

            // 4-tap hemisphere SSAO
            vec2 ts = 1.0/textureSize(uDepthTex, 0);
            float ao = 0.0;
            vec2 offsets[4] = vec2[](
                vec2(1,0), vec2(-1,0), vec2(0,1), vec2(0,-1)
            );
            for (int i = 0; i < 4; ++i) {
                vec2 uv2 = vUV + offsets[i] * ts * uRadius * 50.0;
                float sd = texture(uDepthTex, uv2).r;
                vec3 spos = view_pos(uv2, sd);
                vec3 diff = spos - pos;
                float dist = length(diff);
                float ndotl = max(dot(normal, normalize(diff)), 0.0);
                ao += ndotl * (1.0 / (1.0 + dist*dist)) * step(dist, uRadius*5.0);
            }
            ao /= 4.0;
            fragAO = pow(clamp(ao, 0.0, 1.0), uPower);
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
        ae::log_error(std::string("SSAO shader link: ") + log);
        return false;
    }
    glDeleteShader(vs_id);
    glDeleteShader(fs_id);

    u_proj_ = glGetUniformLocation(shader_, "uProj");
    u_depth_tex_ = glGetUniformLocation(shader_, "uDepthTex");
    u_radius_ = glGetUniformLocation(shader_, "uRadius");
    u_power_ = glGetUniformLocation(shader_, "uPower");
    u_bias_ = glGetUniformLocation(shader_, "uBias");
    return true;
}

void SsaoPass::shutdown() {
    if (shader_) glDeleteProgram(shader_);
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
    if (quad_vao_) glDeleteVertexArrays(1, &quad_vao_);
    if (quad_vbo_) glDeleteBuffers(1, &quad_vbo_);
    if (ao_tex_.id) glDeleteTextures(1, &ao_tex_.id);
    shader_ = 0; fbo_ = 0; quad_vao_ = 0; quad_vbo_ = 0; ao_tex_.id = 0;
}

void SsaoPass::render(GLuint depth_texture, int depth_slot,
                      const float* projection_matrix, int width, int height) {
    if (!shader_ || !fbo_) return;

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width, height);
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shader_);
    glActiveTexture(GL_TEXTURE0 + depth_slot);
    glBindTexture(GL_TEXTURE_2D, depth_texture);
    glUniform1i(u_depth_tex_, depth_slot);
    glUniformMatrix4fv(u_proj_, 1, GL_FALSE, projection_matrix);
    glUniform1f(u_radius_, radius);
    glUniform1f(u_power_, power);
    glUniform1f(u_bias_, bias);

    glBindVertexArray(quad_vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SsaoPass::bind_ao_result(int slot) {
    if (ao_tex_.id) {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, ao_tex_.id);
    }
}

} // namespace ae::render
