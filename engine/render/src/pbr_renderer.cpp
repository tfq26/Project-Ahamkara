#include "ae/render/pbr_renderer.h"
#include "ae/core/log.h"

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

#include <cmath>


#define AE_LOG_CATEGORY "Render"

namespace ae::render {

struct PbrRenderer::Impl {
    RenderBackend* backend = nullptr;
    ShaderHandle pbr_shader;
    PbrLight light;
    GLuint vao = 0;

    // Uniform locations
    int u_model = -1, u_view = -1, u_projection = -1;
    int u_normal_matrix = -1, u_light_space = -1;
    int u_light_dir = -1, u_light_color = -1, u_view_pos = -1;
    int u_albedo = -1, u_metallic = -1, u_roughness = -1;
    int u_ambient = -1;
    int u_has_albedo = -1, u_has_normal = -1, u_has_orm = -1;
    int u_has_skinning = -1, u_joint_matrices = -1;
    int u_albedo_tex = -1, u_normal_tex = -1, u_orm_tex = -1, u_shadow_tex = -1;

    float view[16] = {};
    float proj[16] = {};
    float cam_pos[3] = {};
    ShadowPass* shadow_pass = nullptr;

    bool initialize(RenderBackend* be) {
        backend = be;

        const char* vert_src = R"(
            #version 330 core
            layout(location = 0) in vec3 aPosition;
            layout(location = 1) in vec3 aNormal;
            layout(location = 2) in vec4 aJointIndices;
            layout(location = 3) in vec4 aJointWeights;
            layout(location = 4) in vec2 aTexCoord;
            uniform mat4 uModel, uView, uProjection, uLightSpace;
            uniform mat3 uNormalMatrix;
            uniform bool uHasSkinning;
            uniform mat4 uJointMatrices[64];
            out vec3 vWorldPos, vNormal;
            out vec2 vUV;
            out vec4 vLightSpacePos;
            void main() {
                mat4 model = uModel;
                vec4 wp; vec3 n;
                if (uHasSkinning) {
                    mat4 skin = aJointWeights.x * uJointMatrices[int(aJointIndices.x)]
                              + aJointWeights.y * uJointMatrices[int(aJointIndices.y)]
                              + aJointWeights.z * uJointMatrices[int(aJointIndices.z)]
                              + aJointWeights.w * uJointMatrices[int(aJointIndices.w)];
                    wp = model * skin * vec4(aPosition, 1.0);
                    n = mat3(model) * mat3(skin) * aNormal;
                } else {
                    wp = model * vec4(aPosition, 1.0);
                    n = mat3(model) * aNormal;
                }
                gl_Position = uProjection * uView * wp;
                vWorldPos = wp.xyz;
                vNormal = normalize(n);
                vUV = aTexCoord;
                vLightSpacePos = uLightSpace * wp;
            }
        )";

        const char* frag_src = R"(
            #version 330 core
            in vec3 vWorldPos, vNormal;
            in vec2 vUV;
            in vec4 vLightSpacePos;
            uniform sampler2D uAlbedoMap, uNormalMap, uOrmMap, uShadowMap;
            uniform vec3 uLightDir, uLightColor, uViewPos, uAlbedo;
            uniform float uMetallic, uRoughness, uAmbientStrength;
            uniform bool uHasAlbedoMap, uHasNormalMap, uHasOrmMap;
            out vec4 fragColor;
            const float PI = 3.14159265359;
            float D_GGX(vec3 N, vec3 H, float a) {
                float a2 = a*a; float NdH = max(dot(N,H),0.0);
                float d = NdH*NdH*(a2-1.0)+1.0; return a2/(PI*d*d);
            }
            float G_Smith(vec3 N, vec3 V, vec3 L, float r) {
                float k = (r+1.0)*(r+1.0)/8.0;
                return (max(dot(N,V),0.0)/(max(dot(N,V),0.0)*(1.0-k)+k)) *
                       (max(dot(N,L),0.0)/(max(dot(N,L),0.0)*(1.0-k)+k));
            }
            vec3 F_Schlick(float c, vec3 F0) { return F0 + (1.0-F0)*pow(1.0-c,5.0); }
            float shadow_pcf(vec4 lsp) {
                vec3 p = lsp.xyz/lsp.w*0.5+0.5; if(p.z>1.0) return 0.0;
                float s=0.0; vec2 ts=1.0/textureSize(uShadowMap,0);
                for(int x=-1;x<=1;++x) for(int y=-1;y<=1;++y)
                    s += (p.z-0.005 > texture(uShadowMap,p.xy+vec2(x,y)*ts).r) ? 1.0 : 0.0;
                return s/9.0;
            }
            void main() {
                vec3 alb = uHasAlbedoMap ? texture(uAlbedoMap,vUV).rgb : uAlbedo;
                float met = uHasOrmMap ? texture(uOrmMap,vUV).b : uMetallic;
                float rou = clamp(uHasOrmMap ? texture(uOrmMap,vUV).g : uRoughness, 0.04, 1.0);
                vec3 N = normalize(vNormal);
                vec3 V = normalize(uViewPos - vWorldPos);
                vec3 L = normalize(uLightDir), H = normalize(V+L);
                vec3 F0 = mix(vec3(0.04), alb, met);
                vec3 F = F_Schlick(max(dot(H,V),0.0), F0);
                float D = D_GGX(N,H,rou), G = G_Smith(N,V,L,rou);
                vec3 spec = D*G*F / (4.0*max(dot(N,V),0.0)*max(dot(N,L),0.0)+0.0001);
                vec3 kD = (1.0-F)*(1.0-met);
                float NdotL = max(dot(N,L),0.0);
                vec3 diff = kD*alb/PI;
                float sh = shadow_pcf(vLightSpacePos);
                vec3 amb = alb*uAmbientStrength;
                fragColor = vec4(amb + (1.0-sh)*(diff+spec)*uLightColor*NdotL, 1.0);
            }
        )";

        int aloc[] = {0, 1, 2, 3};
        const char* anam[] = {"aPosition", "aNormal", "aJointIndices", "aJointWeights"};
        ShaderProgramDesc desc{vert_src, frag_src, aloc, anam, 4};
        pbr_shader = backend->create_shader_program(desc);
        if (!pbr_shader) return false;
        glGenVertexArrays(1, &vao);

        u_model     = backend->get_uniform_location(pbr_shader, "uModel");
        u_view      = backend->get_uniform_location(pbr_shader, "uView");
        u_projection= backend->get_uniform_location(pbr_shader, "uProjection");
        u_light_space = backend->get_uniform_location(pbr_shader, "uLightSpace");
        u_normal_matrix=backend->get_uniform_location(pbr_shader, "uNormalMatrix");
        u_light_dir = backend->get_uniform_location(pbr_shader, "uLightDir");
        u_light_color=backend->get_uniform_location(pbr_shader, "uLightColor");
        u_view_pos  = backend->get_uniform_location(pbr_shader, "uViewPos");
        u_albedo    = backend->get_uniform_location(pbr_shader, "uAlbedo");
        u_metallic  = backend->get_uniform_location(pbr_shader, "uMetallic");
        u_roughness = backend->get_uniform_location(pbr_shader, "uRoughness");
        u_ambient   = backend->get_uniform_location(pbr_shader, "uAmbientStrength");
        u_has_albedo= backend->get_uniform_location(pbr_shader, "uHasAlbedoMap");
        u_has_normal= backend->get_uniform_location(pbr_shader, "uHasNormalMap");
        u_has_orm   = backend->get_uniform_location(pbr_shader, "uHasOrmMap");
        u_has_skinning=backend->get_uniform_location(pbr_shader, "uHasSkinning");
        u_joint_matrices=backend->get_uniform_location(pbr_shader, "uJointMatrices");
        u_albedo_tex= backend->get_uniform_location(pbr_shader, "uAlbedoMap");
        u_normal_tex= backend->get_uniform_location(pbr_shader, "uNormalMap");
        u_orm_tex   = backend->get_uniform_location(pbr_shader, "uOrmMap");
        u_shadow_tex= backend->get_uniform_location(pbr_shader, "uShadowMap");

        std::memcpy(view, identity_matrix(), 64);
        std::memcpy(proj, identity_matrix(), 64);
        return true;
    }

    static const float* identity_matrix() {
        static const float m[16] = {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        return m;
    }

    static void mat3_from_mat4(const float* m4, float* m3) {
        m3[0]=m4[0]; m3[1]=m4[1]; m3[2]=m4[2];
        m3[3]=m4[4]; m3[4]=m4[5]; m3[5]=m4[6];
        m3[6]=m4[8]; m3[7]=m4[9]; m3[8]=m4[10];
    }

    void begin_frame(const float* v, const float* p, const float* cp, ShadowPass* sp) {
        std::memcpy(view, v, 64);
        std::memcpy(proj, p, 64);
        std::memcpy(cam_pos, cp, 12);
        shadow_pass = sp;
    }

    void submit(const PbrDrawCall& dc) {
        if (!pbr_shader || vao == 0) return;
        backend->use_shader(pbr_shader);
        glBindVertexArray(vao);

        // Per-frame uniforms
        glUniformMatrix4fv(u_view, 1, GL_FALSE, view);
        glUniformMatrix4fv(u_projection, 1, GL_FALSE, proj);
        glUniform3fv(u_view_pos, 1, cam_pos);
        glUniform3fv(u_light_dir, 1, light.direction);
        glUniform3fv(u_light_color, 1, light.color);
        glUniform1f(u_ambient, light.ambient);

        // Light space matrix
        if (shadow_pass) {
            float lv[16] = {}, lp[16] = {};
            // Build light view (look-at from light direction)
            float up[3] = {0, 1, 0};
            float fwd[3] = {light.direction[0], light.direction[1], light.direction[2]};
            float len = std::sqrt(fwd[0]*fwd[0]+fwd[1]*fwd[1]+fwd[2]*fwd[2]);
            if (len > 0) { fwd[0]/=len; fwd[1]/=len; fwd[2]/=len; }
            float rt[3] = {up[1]*fwd[2]-up[2]*fwd[1], up[2]*fwd[0]-up[0]*fwd[2], up[0]*fwd[1]-up[1]*fwd[0]};
            len = std::sqrt(rt[0]*rt[0]+rt[1]*rt[1]+rt[2]*rt[2]);
            if (len > 0) { rt[0]/=len; rt[1]/=len; rt[2]/=len; }
            float up2[3] = {fwd[1]*rt[2]-fwd[2]*rt[1], fwd[2]*rt[0]-fwd[0]*rt[2], fwd[0]*rt[1]-fwd[1]*rt[0]};

            float dist = 40.0F;
            float eye[3] = {cam_pos[0]-fwd[0]*dist, cam_pos[1]-fwd[1]*dist, cam_pos[2]-fwd[2]*dist};
            lv[0]=rt[0]; lv[1]=up2[0]; lv[2]=-fwd[0]; lv[3]=0;
            lv[4]=rt[1]; lv[5]=up2[1]; lv[6]=-fwd[1]; lv[7]=0;
            lv[8]=rt[2]; lv[9]=up2[2]; lv[10]=-fwd[2]; lv[11]=0;
            lv[12]=-(rt[0]*eye[0]+rt[1]*eye[1]+rt[2]*eye[2]);
            lv[13]=-(up2[0]*eye[0]+up2[1]*eye[1]+up2[2]*eye[2]);
            lv[14]=-(-fwd[0]*eye[0]-fwd[1]*eye[1]-fwd[2]*eye[2]);
            lv[15]=1;

            float n=1.0F, f=100.0F, s=30.0F;
            lp[0]=1/s; lp[5]=1/s; lp[10]=-2/(f-n); lp[11]=0;
            lp[14]=-(f+n)/(f-n); lp[15]=1;

            float ls[16];
            for (int i=0;i<4;++i) for(int j=0;j<4;++j) {
                ls[i*4+j]=lp[i*4+0]*lv[0*4+j]+lp[i*4+1]*lv[1*4+j]+lp[i*4+2]*lv[2*4+j]+lp[i*4+3]*lv[3*4+j];
            }
            glUniformMatrix4fv(u_light_space, 1, GL_FALSE, ls);
            shadow_pass->bind_shadow_map(3);
            glUniform1i(u_shadow_tex, 3);
        }

        // Per-draw uniforms
        glUniformMatrix4fv(u_model, 1, GL_FALSE, dc.model_matrix);
        float nm[9]; mat3_from_mat4(dc.model_matrix, nm);
        glUniformMatrix3fv(u_normal_matrix, 1, GL_FALSE, nm);
        glUniform3fv(u_albedo, 1, dc.albedo);
        glUniform1f(u_metallic, dc.metallic);
        glUniform1f(u_roughness, dc.roughness);

        bool has_alb = dc.albedo_map.id != 0;
        bool has_nrm = dc.normal_map.id != 0;
        bool has_orm = dc.orm_map.id != 0;
        glUniform1i(u_has_albedo, has_alb ? 1 : 0);
        glUniform1i(u_has_normal, has_nrm ? 1 : 0);
        glUniform1i(u_has_orm, has_orm ? 1 : 0);

        if (has_alb) { backend->bind_texture(dc.albedo_map, 0); glUniform1i(u_albedo_tex, 0); }
        if (has_nrm) { backend->bind_texture(dc.normal_map, 1); glUniform1i(u_normal_tex, 1); }
        if (has_orm) { backend->bind_texture(dc.orm_map, 2); glUniform1i(u_orm_tex, 2); }

        bool skin = dc.joint_matrices != nullptr && dc.joint_count > 0;
        glUniform1i(u_has_skinning, skin ? 1 : 0);
        if (skin) glUniformMatrix4fv(u_joint_matrices, dc.joint_count, GL_FALSE, dc.joint_matrices);

        // Draw
        auto& mesh = *dc.mesh;
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo_positions.id);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray(0);

        if (mesh.vbo_normals.id) {
            glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo_normals.id);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
            glEnableVertexAttribArray(1);
        }

        bool has_uv = mesh.vbo_texcoords.id != 0;
        if (has_uv) {
            glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo_texcoords.id);
            glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
            glEnableVertexAttribArray(4);
        }

        if (mesh.vbo_joints.id && skin) {
            glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo_joints.id);
            glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
            glEnableVertexAttribArray(2);
            glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo_weights.id);
            glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
            glEnableVertexAttribArray(3);
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibo_indices.id);
        glDrawElements(GL_TRIANGLES, mesh.index_count, GL_UNSIGNED_INT, nullptr);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        if (has_uv) glDisableVertexAttribArray(4);
        if (skin) { glDisableVertexAttribArray(2); glDisableVertexAttribArray(3); }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void shutdown() {
        if (vao != 0) {
            glDeleteVertexArrays(1, &vao);
            vao = 0;
        }
        if (pbr_shader) {
            backend->destroy_shader(pbr_shader);
            pbr_shader = {};
        }
    }

    void end_frame() {}
};

PbrRenderer::PbrRenderer() : impl_(std::make_unique<Impl>()) {}
PbrRenderer::~PbrRenderer() = default;
bool PbrRenderer::initialize(RenderBackend* backend) { return impl_->initialize(backend); }
void PbrRenderer::shutdown() { impl_->shutdown(); }
void PbrRenderer::begin_frame(const float* v, const float* p, const float* c, ShadowPass* s) { impl_->begin_frame(v, p, c, s); }
void PbrRenderer::submit(const PbrDrawCall& dc) { impl_->submit(dc); }
void PbrRenderer::end_frame() { impl_->end_frame(); }

} // namespace ae::render
