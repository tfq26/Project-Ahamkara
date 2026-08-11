#include "ae/render/shadow_pass.h"
#include "ae/core/log.h"
#include "ae/render/gl_platform.h"

#include <cmath>
#include <cstring>

#define AE_LOG_CATEGORY "Render"

namespace ae::render {

// ── Static helpers ──────────────────────────────────────────────────────────

// Build a right-handed look-at matrix (column-major, 4×4) from an eye point,
// a target point, and an up vector.
static void look_at(const float eye[3], const float target[3], const float up[3], float out[16]) {
    float fwd[3] = {target[0]-eye[0], target[1]-eye[1], target[2]-eye[2]};
    float len = std::sqrt(fwd[0]*fwd[0]+fwd[1]*fwd[1]+fwd[2]*fwd[2]);
    if (len > 1e-8f) { fwd[0]/=len; fwd[1]/=len; fwd[2]/=len; }
    float rt[3] = {up[1]*fwd[2]-up[2]*fwd[1], up[2]*fwd[0]-up[0]*fwd[2], up[0]*fwd[1]-up[1]*fwd[0]};
    len = std::sqrt(rt[0]*rt[0]+rt[1]*rt[1]+rt[2]*rt[2]);
    if (len > 1e-8f) { rt[0]/=len; rt[1]/=len; rt[2]/=len; }
    float up2[3] = {fwd[1]*rt[2]-fwd[2]*rt[1], fwd[2]*rt[0]-fwd[0]*rt[2], fwd[0]*rt[1]-fwd[1]*rt[0]};
    out[0]=rt[0]; out[1]=up2[0]; out[2]=-fwd[0]; out[3]=0;
    out[4]=rt[1]; out[5]=up2[1]; out[6]=-fwd[1]; out[7]=0;
    out[8]=rt[2]; out[9]=up2[2]; out[10]=-fwd[2]; out[11]=0;
    out[12]=-(rt[0]*eye[0]+rt[1]*eye[1]+rt[2]*eye[2]);
    out[13]=-(up2[0]*eye[0]+up2[1]*eye[1]+up2[2]*eye[2]);
    out[14]=-(-fwd[0]*eye[0]-fwd[1]*eye[1]-fwd[2]*eye[2]);
    out[15]=1;
}

// Build an orthographic projection matrix (column-major).
static void ortho(float left, float right, float bottom, float top, float near_p, float far_p, float out[16]) {
    std::memset(out, 0, 16*sizeof(float));
    out[0]=2.0f/(right-left);
    out[5]=2.0f/(top-bottom);
    out[10]=-2.0f/(far_p-near_p);
    out[12]=-(right+left)/(right-left);
    out[13]=-(top+bottom)/(top-bottom);
    out[14]=-(far_p+near_p)/(far_p-near_p);
    out[15]=1;
}

// Multiply two 4×4 column-major matrices: out = a * b
static void mat_mul(const float a[16], const float b[16], float out[16]) {
    for (int i=0;i<4;++i) for(int j=0;j<4;++j) {
        out[i*4+j]=a[i*4+0]*b[0*4+j]+a[i*4+1]*b[1*4+j]+a[i*4+2]*b[2*4+j]+a[i*4+3]*b[3*4+j];
    }
}

struct ShadowPass::Impl {
    RenderBackend* backend = nullptr;
    GLuint fbo = 0;
    GLuint vao = 0;
    GLuint cube_vbo = 0;
    GLuint cube_ibo = 0;
    TextureHandle depth_map;
    // CSM: array texture holds all cascades as layers
    GLuint csm_depth_array = 0;
    GLuint csm_fbo = 0;
    ShaderHandle shader;
    int resolution = 2048;
    int u_light_space_loc = -1;
    int u_model_loc = -1;
    int current_cascade_ = -1;

    bool initialize(RenderBackend* be, int res) {
        backend = be;
        resolution = res;

        // Single-cascade FBO
        glGenFramebuffers(1, &fbo);
        glGenVertexArrays(1, &vao);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

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
        depth_map.id = depth_tex;

        // CSM array texture + FBO (for up to kMaxCsmCascades layers)
        glGenTextures(1, &csm_depth_array);
        glBindTexture(GL_TEXTURE_2D_ARRAY, csm_depth_array);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F,
                     resolution, resolution, kMaxCsmCascades, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

        glGenFramebuffers(1, &csm_fbo);

        // Cube geometry
        static const float cube_positions[] = {
            0.0f,0.0f,0.0f, 1.0f,0.0f,0.0f, 1.0f,1.0f,0.0f, 0.0f,1.0f,0.0f,
            0.0f,0.0f,1.0f, 1.0f,0.0f,1.0f, 1.0f,1.0f,1.0f, 0.0f,1.0f,1.0f,
        };
        static const GLuint cube_indices[] = {
            0,1,2,2,3,0, 4,5,6,6,7,4, 0,4,7,7,3,0,
            1,5,6,6,2,1, 3,2,6,6,7,3, 0,1,5,5,4,0,
        };
        glGenBuffers(1, &cube_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cube_positions), cube_positions, GL_STATIC_DRAW);
        glGenBuffers(1, &cube_ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube_ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        // Shadow shader
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
        if (csm_fbo) { glDeleteFramebuffers(1, &csm_fbo); csm_fbo = 0; }
        if (csm_depth_array) { glDeleteTextures(1, &csm_depth_array); csm_depth_array = 0; }
        if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
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
            float light_space[16];
            mat_mul(light_projection, light_view, light_space);
            glUniformMatrix4fv(u_light_space_loc, 1, GL_FALSE, light_space);
        }
    }

    void begin_csm_pass(const CsmData& csm, const float* cam_view,
                        const float* cam_proj, const float* cam_pos) {
        (void)cam_view; (void)cam_proj; (void)cam_pos;
        backend->use_shader(shader);
        backend->set_depth_test(true);
        backend->set_depth_write(true);
        backend->set_color_write(false, false, false, false);
    }

    void end_pass() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        backend->set_color_write(true, true, true, true);
    }

    void submit_box_caster(const ShadowBoxCaster& caster) {
        if (!shader || vao == 0 || cube_vbo == 0 || cube_ibo == 0 || u_model_loc == -1) {
            return;
        }
        const float sx = caster.max[0] - caster.min[0];
        const float sy = caster.max[1] - caster.min[1];
        const float sz = caster.max[2] - caster.min[2];
        const float model[16] = {
            sx,0,0,0, 0,sy,0,0, 0,0,sz,0,
            caster.min[0],caster.min[1],caster.min[2],1
        };
        glUniformMatrix4fv(u_model_loc, 1, GL_FALSE, model);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube_ibo);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
        glDisableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void bind_shadow_map(int slot, int cascade_index) {
        glActiveTexture(GL_TEXTURE0 + slot);
        if (cascade_index >= 0 && csm_depth_array != 0) {
            // Bind a specific CSM cascade layer as a 2D view
            glBindTexture(GL_TEXTURE_2D_ARRAY, csm_depth_array);
            // We bind the array; the shader uses the layer via gl_Layer
        } else if (csm_depth_array != 0) {
            glBindTexture(GL_TEXTURE_2D_ARRAY, csm_depth_array);
        } else {
            glBindTexture(GL_TEXTURE_2D, depth_map.id);
        }
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
void ShadowPass::begin_csm_pass(const CsmData& csm, const float* cam_view,
                                const float* cam_proj, const float* cam_pos) {
    impl_->begin_csm_pass(csm, cam_view, cam_proj, cam_pos);
}
void ShadowPass::end_pass() { impl_->end_pass(); }
void ShadowPass::submit_box_caster(const ShadowBoxCaster& caster) { impl_->submit_box_caster(caster); }
void ShadowPass::bind_shadow_map(int slot, int cascade_index) { impl_->bind_shadow_map(slot, cascade_index); }

} // namespace ae::render
