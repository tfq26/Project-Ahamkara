#include "ae/render/shadow_pass.h"
#include "ae/render/render_backend.h"

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace ae::render {

struct ShadowPass::Impl {
    RenderBackend* backend = nullptr;
    GLuint fbo = 0;
    GLuint cube_vbo = 0;
    GLuint cube_ibo = 0;
    TextureHandle depth_map;
    ShaderHandle shader;
    int resolution = 2048;
    int u_light_space_loc = -1;
    int u_model_loc = -1;

    bool initialize(RenderBackend* be, int res) {
        backend = be;
        resolution = res;

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        // Create depth texture
        GLuint depth_tex = 0;
        glGenTextures(1, &depth_tex);
        glBindTexture(GL_TEXTURE_2D, depth_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float border[] = {1.0f, 1.0f, 1.0f, 1.0f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_tex, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        depth_map = {1}; // store raw GLuint
        depth_map.id = depth_tex;

        static const float cube_positions[] = {
            0.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f,
            1.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 1.0f,
            1.0f, 0.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            0.0f, 1.0f, 1.0f,
        };
        static const GLuint cube_indices[] = {
            0, 1, 2, 2, 3, 0,
            4, 5, 6, 6, 7, 4,
            0, 4, 7, 7, 3, 0,
            1, 5, 6, 6, 2, 1,
            3, 2, 6, 6, 7, 3,
            0, 1, 5, 5, 4, 0,
        };

        glGenBuffers(1, &cube_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cube_positions), cube_positions, GL_STATIC_DRAW);
        glGenBuffers(1, &cube_ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube_ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        // Compile shadow shader
        const char* vert_src = R"(
            #version 330 core
            layout(location = 0) in vec3 aPosition;
            uniform mat4 uLightSpace;
            uniform mat4 uModel;
            void main() { gl_Position = uLightSpace * uModel * vec4(aPosition, 1.0); }
        )";
        const char* frag_src = R"(
            #version 330 core
            void main() {}
        )";

        int attrib_locs[] = {0};
        const char* attrib_names[] = {"aPosition"};
        ShaderProgramDesc desc{vert_src, frag_src, attrib_locs, attrib_names, 1};
        shader = backend->create_shader_program(desc);
        if (!shader) return false;

        u_light_space_loc = backend->get_uniform_location(shader, "uLightSpace");
        u_model_loc = backend->get_uniform_location(shader, "uModel");
        return true;
    }

    void shutdown() {
        if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
        if (cube_vbo) { glDeleteBuffers(1, &cube_vbo); cube_vbo = 0; }
        if (cube_ibo) { glDeleteBuffers(1, &cube_ibo); cube_ibo = 0; }
        if (shader) { backend->destroy_shader(shader); shader = {}; }
        if (depth_map.id > 0) { glDeleteTextures(1, &depth_map.id); depth_map = {}; }
    }

    void begin_pass(const float* light_view, const float* light_projection) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, resolution, resolution);
        glClear(GL_DEPTH_BUFFER_BIT);
        backend->set_depth_test(true);
        backend->set_depth_write(true);
        backend->set_color_write(false, false, false, false);

        backend->use_shader(shader);
        if (u_light_space_loc != -1) {
            // Combine light view * projection
            float light_space[16];
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    light_space[i * 4 + j] =
                        light_projection[i * 4 + 0] * light_view[0 * 4 + j] +
                        light_projection[i * 4 + 1] * light_view[1 * 4 + j] +
                        light_projection[i * 4 + 2] * light_view[2 * 4 + j] +
                        light_projection[i * 4 + 3] * light_view[3 * 4 + j];
            glUniformMatrix4fv(u_light_space_loc, 1, GL_FALSE, light_space);
        }
    }

    void end_pass() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        backend->set_color_write(true, true, true, true);
    }

    void submit_box_caster(const ShadowBoxCaster& caster) {
        if (!shader || cube_vbo == 0 || cube_ibo == 0 || u_model_loc == -1) {
            return;
        }

        const float sx = caster.max[0] - caster.min[0];
        const float sy = caster.max[1] - caster.min[1];
        const float sz = caster.max[2] - caster.min[2];
        const float model[16] = {
            sx,   0,   0, 0,
             0,  sy,   0, 0,
             0,   0,  sz, 0,
            caster.min[0], caster.min[1], caster.min[2], 1
        };

        glUniformMatrix4fv(u_model_loc, 1, GL_FALSE, model);
        glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube_ibo);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
        glDisableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    void bind_shadow_map(int slot) {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, depth_map.id);
    }
};

ShadowPass::ShadowPass() : impl_(std::make_unique<Impl>()) {}
ShadowPass::~ShadowPass() = default;

bool ShadowPass::initialize(RenderBackend* backend, int resolution) {
    return impl_->initialize(backend, resolution);
}

void ShadowPass::shutdown() { impl_->shutdown(); }
void ShadowPass::begin_pass(const float* light_view, const float* light_projection) {
    impl_->begin_pass(light_view, light_projection);
}
void ShadowPass::end_pass() { impl_->end_pass(); }
void ShadowPass::submit_box_caster(const ShadowBoxCaster& caster) { impl_->submit_box_caster(caster); }
void ShadowPass::bind_shadow_map(int slot) { impl_->bind_shadow_map(slot); }

} // namespace ae::render
