#include "ae/render/atmosphere_pass.h"
#include "ae/render/color_grading.h"
#include "ae/render/pbr_renderer.h"
#include "ae/render/ssao_pass.h"
#include "ae/render/temporal_aa.h"
#include "ae/core/log.h"
#include "ae/render/gl_platform.h"

#include <cmath>
#include <cstring>

#define AE_LOG_CATEGORY "Render"

namespace ae::render {

struct PbrRenderer::Impl {
    RenderBackend* backend = nullptr;
    ShaderHandle pbr_shader;
    PbrLight lights[kMaxLights];
    int light_count = 1;
    float cascade_splits[kCsmCascadeCount] = {10.0F, 30.0F, 60.0F};
    GLuint vao = 0;

    // Uniform locations
    int u_model = -1, u_view = -1, u_projection = -1;
    int u_normal_matrix = -1, u_light_space = -1;
    int u_light_dir = -1, u_light_color = -1, u_view_pos = -1;
    int u_albedo = -1, u_metallic = -1, u_roughness = -1;
    int u_ambient = -1;
    int u_ambient_sky = -1, u_ambient_ground = -1;
    int u_ao_tex = -1, u_has_ao = -1;
    int u_has_albedo = -1, u_has_normal = -1, u_has_orm = -1;
    int u_has_skinning = -1, u_joint_matrices = -1;
    int u_albedo_tex = -1, u_normal_tex = -1, u_orm_tex = -1, u_shadow_tex = -1;
    // Multi-light / CSM uniform locations
    int u_light_count = -1;
    int u_light_dir_array = -1, u_light_color_array = -1;
    int u_light_pos_array = -1, u_light_intensity = -1;
    int u_light_type_array = -1, u_light_range_array = -1;
    int u_csm_splits = -1, u_csm_matrices = -1;
    int u_csm_cascade_count = -1;
    // Ambient / IBL uniform locations
    int u_emissive_color = -1, u_emissive_intensity = -1, u_has_emissive_map = -1;
    int u_emissive_tex = -1;
    int u_sh_coeffs = -1, u_use_sh_ambient = -1;
    // Reflection probe
    int u_probe_count = -1;
    int u_probe_cubemaps = -1, u_probe_positions = -1;
    int u_probe_radius = -1, u_probe_intensity = -1;
    // Color grading uniforms
    int u_exposure = -1, u_contrast = -1, u_saturation = -1, u_brightness = -1;
    int u_vignette_strength = -1, u_vignette_radius = -1, u_tonemap_mode = -1;
    // Fog uniforms
    int u_height_fog_density = -1, u_height_fog_height = -1, u_height_fog_falloff = -1;
    int u_aerial_fog_density = -1, u_fog_color = -1;
    ColorGradingParams color_grading_;
    FogParams fog_params_;

    // Ambient state

    // SSAO / TAA state
    SsaoPass ssao_pass_;
    TemporalAA taa_;
    bool ssao_enabled_ = false;
    int frame_index_ = 0;
    float jittered_proj_[16] = {};

    // TAA uniform locations
    int u_prev_view = -1, u_prev_proj = -1;

    // Ambient SH coefficients (3rd order, 9 floats)
    float sh_coeffs_[kShCoeffCount] = {};
    bool use_sh_ambient_ = false;
    float ambient_sky_color_[3] = {0.5F, 0.7F, 1.0F};
    float ambient_ground_color_[3] = {0.2F, 0.15F, 0.1F};
    TextureHandle ao_texture_;
    ReflectionProbe reflection_probes_[kMaxReflectionProbes];
    int reflection_probe_count_ = 0;

    float view[16] = {};
    float proj[16] = {};
    float cam_pos[3] = {};
    ShadowPass* shadow_pass = nullptr;

    void set_lights(const PbrLight* src, int count) {
        light_count = std::min(count, kMaxLights);
        for (int i = 0; i < light_count; ++i) std::memcpy(&lights[i], &src[i], sizeof(PbrLight));
    }

    void set_cascade_splits(const float* splits) {
        for (int i = 0; i < kCsmCascadeCount; ++i) cascade_splits[i] = splits[i];
    }

    void set_ambient_sh(const float* coeffs) {
        std::memcpy(sh_coeffs_, coeffs, kShCoeffCount * sizeof(float));
        use_sh_ambient_ = true;
    }

    void set_ambient_sky_ground(const float sky[3], const float ground[3]) {
        ambient_sky_color_[0] = sky[0];
        ambient_sky_color_[1] = sky[1];
        ambient_sky_color_[2] = sky[2];
        ambient_ground_color_[0] = ground[0];
        ambient_ground_color_[1] = ground[1];
        ambient_ground_color_[2] = ground[2];
        use_sh_ambient_ = false;
    }

    void set_ao_texture(TextureHandle tex) {
        ao_texture_ = tex;
    }

    void set_reflection_probes(const ReflectionProbe* probes, int count) {
        reflection_probe_count_ = std::min(count, kMaxReflectionProbes);
        for (int i = 0; i < reflection_probe_count_; ++i)
            reflection_probes_[i] = probes[i];
    }

    void set_color_grading(const ColorGradingParams& params) {
        color_grading_ = params;
    }

    void set_fog(const FogParams& params) {
        fog_params_ = params;
    }

    bool initialize(RenderBackend* be) {
        backend = be;

        // ── Vertex shader: skinning + CSM light-space outputs ─────────────
        const char* vert_src = R"(
            #version 330 core
            layout(location = 0) in vec3 aPosition;
            layout(location = 1) in vec3 aNormal;
            layout(location = 2) in vec4 aJointIndices;
            layout(location = 3) in vec4 aJointWeights;
            layout(location = 4) in vec2 aTexCoord;
            uniform mat4 uModel, uView, uProjection;
            uniform mat4 uCsmMatrices[4];
            uniform int uCsmCascadeCount;
            uniform mat3 uNormalMatrix;
            uniform bool uHasSkinning;
            uniform mat4 uJointMatrices[64];
            out vec3 vWorldPos, vNormal;
            out vec2 vUV;
            out vec4 vCsmPos[4];
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
                for (int i = 0; i < uCsmCascadeCount; ++i)
                    vCsmPos[i] = uCsmMatrices[i] * wp;
            }
        )";

        // ── Fragment shader: multi-light PBR + CSM + IBL ──────────────────
        const char* frag_src = R"(
            #version 330 core
            #define MAX_LIGHTS 12
            #define MAX_CSM 4
            #define MAX_PROBES 4
            #define SH_COEFFS 9
            in vec3 vWorldPos, vNormal;
            in vec2 vUV;
            in vec4 vCsmPos[MAX_CSM];
            uniform sampler2D uAlbedoMap, uNormalMap, uOrmMap, uEmissiveMap;
            uniform sampler2DArray uShadowMapArray;
            uniform samplerCube uProbeCubemaps[MAX_PROBES];
            uniform vec3 uViewPos, uAlbedo, uEmissiveColor;
            uniform vec3 uAmbientSkyColor, uAmbientGroundColor;
            uniform float uMetallic, uRoughness, uAmbientStrength, uEmissiveIntensity;
            uniform bool uHasAlbedoMap, uHasNormalMap, uHasOrmMap, uHasEmissiveMap;
            uniform sampler2D uAOTexture;
            uniform bool uHasAOTexture;
            uniform int uLightCount, uCsmCascadeCount, uUseShAmbient;
            uniform vec3 uLightDirs[MAX_LIGHTS];
            uniform vec3 uLightColors[MAX_LIGHTS];
            uniform vec3 uLightPositions[MAX_LIGHTS];
            uniform float uLightIntensities[MAX_LIGHTS];
            uniform int uLightTypes[MAX_LIGHTS];
            uniform float uLightRanges[MAX_LIGHTS];
            uniform float uCsmSplits[4];
            uniform float uShCoeffs[SH_COEFFS];
            uniform int uProbeCount;
            uniform vec3 uProbePositions[MAX_PROBES];
            uniform float uProbeRadius[MAX_PROBES];
            uniform float uProbeIntensity[MAX_PROBES];
            // SSAO uniforms
            uniform sampler2D uAOTex;
            uniform bool uHasAO;
            // TAA / motion vector uniforms
            uniform mat4 uPrevView, uPrevProj;
            // Color grading uniforms
            uniform float uExposure, uContrast, uSaturation, uBrightness;
            uniform float uVignetteStrength, uVignetteRadius;
            uniform int uTonemapMode;
            // Fog uniforms
            uniform float uHeightFogDensity, uHeightFogHeight, uHeightFogFalloff;
            uniform float uAerialFogDensity;
            uniform vec3 uFogColor;
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

            // Evaluate 3rd-order SH for a given direction
            vec3 sh_eval(vec3 dir, float coeffs[SH_COEFFS]) {
                // Constant (L00)
                vec3 c = coeffs[0] * 0.282095 * vec3(1.0);
                // Linear (L1-1, L10, L11)
                c += coeffs[1] * 0.488603 * dir.y * vec3(1.0);
                c += coeffs[2] * 0.488603 * dir.z * vec3(1.0);
                c += coeffs[3] * 0.488603 * dir.x * vec3(1.0);
                // Quad (L2-2, L2-1, L20, L21, L22)
                c += coeffs[4] * 1.092548 * dir.x * dir.y * vec3(1.0);
                c += coeffs[5] * 1.092548 * dir.y * dir.z * vec3(1.0);
                c += coeffs[6] * 0.315392 * (3.0*dir.z*dir.z - 1.0) * vec3(1.0);
                c += coeffs[7] * 1.092548 * dir.x * dir.z * vec3(1.0);
                c += coeffs[8] * 0.546274 * (dir.x*dir.x - dir.y*dir.y) * vec3(1.0);
                return max(c, vec3(0.0));
            }

            float csm_shadow(int cascade, vec4 lsp) {
                vec3 p = lsp.xyz/lsp.w*0.5+0.5;
                if (p.z > 1.0) return 0.0;
                float s=0.0; vec2 ts=1.0/textureSize(uShadowMapArray,0).xy;
                for(int x=-1;x<=1;++x) for(int y=-1;y<=1;++y)
                    s += (p.z-0.005 > texture(uShadowMapArray,vec3(p.xy+vec2(x,y)*ts,cascade)).r) ? 1.0 : 0.0;
                return s/9.0;
            }

            float compute_shadow(float view_z) {
                int cascade = 0;
                float abs_z = abs(view_z);
                for (int i = uCsmCascadeCount-1; i >= 0; --i) {
                    if (abs_z <= uCsmSplits[i]) { cascade = i; break; }
                }
                return csm_shadow(cascade, vCsmPos[cascade]);
            }

            void main() {
                vec3 alb = uHasAlbedoMap ? texture(uAlbedoMap,vUV).rgb : uAlbedo;
                float met = uHasOrmMap ? texture(uOrmMap,vUV).b : uMetallic;
                float rou = clamp(uHasOrmMap ? texture(uOrmMap,vUV).g : uRoughness, 0.04, 1.0);
                vec3 N = normalize(vNormal);
                vec3 V = normalize(uViewPos - vWorldPos);
                vec3 F0 = mix(vec3(0.04), alb, met);

                // View-space Z for cascade selection
                float view_z = length(uViewPos - vWorldPos);
                float shadow_factor = compute_shadow(view_z);

                // ── Direct lighting ──
                vec3 total_light = vec3(0.0);
                for (int i = 0; i < uLightCount; ++i) {
                    vec3 L, radiance;
                    float NdotL;
                    if (uLightTypes[i] == 0) {
                        L = normalize(uLightDirs[i]);
                        radiance = uLightColors[i] * uLightIntensities[i];
                    } else {
                        vec3 delta = uLightPositions[i] - vWorldPos;
                        float dist = length(delta);
                        if (dist > uLightRanges[i]) continue;
                        L = delta / dist;
                        float atten = clamp(1.0 - dist*dist/(uLightRanges[i]*uLightRanges[i]), 0.0, 1.0);
                        atten *= atten;
                        radiance = uLightColors[i] * uLightIntensities[i] * atten;
                    }
                    NdotL = max(dot(N, L), 0.0);
                    if (NdotL <= 0.0) continue;
                    vec3 H = normalize(V + L);
                    vec3 F = F_Schlick(max(dot(H,V),0.0), F0);
                    float D = D_GGX(N, H, rou);
                    float G = G_Smith(N, V, L, rou);
                    vec3 spec = D*G*F / (4.0*max(dot(N,V),0.0)*NdotL+0.0001);
                    vec3 kD = (1.0-F)*(1.0-met);
                    vec3 diff = kD*alb/PI;
                    float sh = (i == 0) ? shadow_factor : 0.0;
                    total_light += (1.0-sh)*(diff+spec)*radiance*NdotL;
                }

                // ── Ambient (SH irradiance or hemispherical) ──
                vec3 ambient;
                float occlusion = 1.0;
                if (uHasAOTexture) {
                    occlusion = texture(uAOTexture, vUV).r;
                }
                if (uUseShAmbient == 1) {
                    float shc[SH_COEFFS];
                    for (int i = 0; i < SH_COEFFS; ++i) shc[i] = uShCoeffs[i];
                    ambient = alb * sh_eval(N, shc) * occlusion;
                } else {
                    float hemisphere = 0.5 + 0.5 * N.y;
                    vec3 hemi_color = mix(uAmbientGroundColor, uAmbientSkyColor, hemisphere);
                    ambient = alb * uAmbientStrength * hemi_color * occlusion;
                }

                // SSAO factor — reduces ambient in occluded areas
                float ao_factor = 1.0;
                if (uHasAO) {
                    ao_factor = 1.0 - texture(uAOTex, vUV).r;
                    ao_factor = clamp(ao_factor, 0.0, 1.0);
                }
                ambient *= ao_factor;

                // ── IBL / reflection probes ──
                vec3 ibl_reflection = vec3(0.0);
                vec3 R = reflect(-V, N);
                for (int i = 0; i < uProbeCount; ++i) {
                    vec3 delta = uProbePositions[i] - vWorldPos;
                    float dist = length(delta);
                    if (dist > uProbeRadius[i]) continue;
                    float weight = clamp(1.0 - dist / uProbeRadius[i], 0.0, 1.0);
                    // Simple cubemap lookup (no mip-level selection without pre-filter)
                    vec3 probe_col = texture(uProbeCubemaps[i], R).rgb;
                    ibl_reflection += probe_col * weight * uProbeIntensity[i];
                }

                // Simple Fresnel for IBL reflection (split-sum approximation)
                float NdotV = max(dot(N, V), 0.0);
                vec3 F_ibl = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);
                vec3 kS_ibl = F_ibl;
                vec3 kD_ibl = (1.0 - kS_ibl) * (1.0 - met);
                vec3 diffuse_ibl = kD_ibl * ambient;
                vec3 specular_ibl = kS_ibl * ibl_reflection;

                // ── Emissive ──
                vec3 emissive = uEmissiveColor * uEmissiveIntensity;
                if (uHasEmissiveMap) emissive += texture(uEmissiveMap, vUV).rgb * uEmissiveIntensity;

                vec3 color = diffuse_ibl + specular_ibl + total_light + emissive;

                // ── Color grading ──
                // Exposure
                color *= uExposure;
                // Contrast (mid-gray pivot)
                color = (color - 0.5) * uContrast + 0.5;
                // Saturation
                float lum = dot(color, vec3(0.299, 0.587, 0.114));
                color = mix(vec3(lum), color, uSaturation);
                // Brightness
                color += uBrightness;

                // Tonemapping
                if (uTonemapMode == 1) {
                    // Reinhard
                    color = color / (color + vec3(1.0));
                } else if (uTonemapMode == 2) {
                    // ACES filmic (approx)
                    vec3 a = color * (color + 0.0245786) - 0.000090537;
                    vec3 b = color * (0.983729 * color + 0.4329510) + 0.238081;
                    color = a / b;
                }

                // Vignette
                vec2 uv_center = vUV - 0.5;
                float vignette = 1.0 - dot(uv_center, uv_center) * uVignetteStrength;
                vignette = clamp(vignette, 0.0, 1.0);
                color *= smoothstep(uVignetteRadius, 1.0, vignette);

                // ── Fog (height + aerial) ──
                float dist = length(uViewPos - vWorldPos);
                // Height fog: thickest at uHeightFogHeight, thins with uHeightFogFalloff
                float height_factor = exp(-abs(vWorldPos.y - uHeightFogHeight) / max(uHeightFogFalloff, 0.01));
                float height_fog = 1.0 - exp(-dist * uHeightFogDensity * height_factor);
                // Aerial (distance) fog
                float aerial_fog = 1.0 - exp(-dist * uAerialFogDensity);
                float fog_amount = clamp(max(height_fog, aerial_fog), 0.0, 1.0);
                color = mix(color, uFogColor, fog_amount);

                fragColor = vec4(color, 1.0);
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
        u_normal_matrix=backend->get_uniform_location(pbr_shader, "uNormalMatrix");
        u_view_pos  = backend->get_uniform_location(pbr_shader, "uViewPos");
        u_albedo    = backend->get_uniform_location(pbr_shader, "uAlbedo");
        u_metallic  = backend->get_uniform_location(pbr_shader, "uMetallic");
        u_roughness = backend->get_uniform_location(pbr_shader, "uRoughness");
        u_ambient   = backend->get_uniform_location(pbr_shader, "uAmbientStrength");
        u_ambient_sky = backend->get_uniform_location(pbr_shader, "uAmbientSkyColor");
        u_ambient_ground = backend->get_uniform_location(pbr_shader, "uAmbientGroundColor");
        u_ao_tex = backend->get_uniform_location(pbr_shader, "uAOTexture");
        u_has_ao = backend->get_uniform_location(pbr_shader, "uHasAOTexture");
        u_has_albedo= backend->get_uniform_location(pbr_shader, "uHasAlbedoMap");
        u_has_normal= backend->get_uniform_location(pbr_shader, "uHasNormalMap");
        u_has_orm   = backend->get_uniform_location(pbr_shader, "uHasOrmMap");
        u_has_skinning=backend->get_uniform_location(pbr_shader, "uHasSkinning");
        u_joint_matrices=backend->get_uniform_location(pbr_shader, "uJointMatrices");
        u_albedo_tex= backend->get_uniform_location(pbr_shader, "uAlbedoMap");
        u_normal_tex= backend->get_uniform_location(pbr_shader, "uNormalMap");
        u_orm_tex   = backend->get_uniform_location(pbr_shader, "uOrmMap");
        u_shadow_tex= backend->get_uniform_location(pbr_shader, "uShadowMapArray");

        // Multi-light / CSM uniforms
        u_light_count = backend->get_uniform_location(pbr_shader, "uLightCount");
        u_light_dir_array = backend->get_uniform_location(pbr_shader, "uLightDirs");
        u_light_color_array = backend->get_uniform_location(pbr_shader, "uLightColors");
        u_light_pos_array = backend->get_uniform_location(pbr_shader, "uLightPositions");
        u_light_intensity = backend->get_uniform_location(pbr_shader, "uLightIntensities");
        u_light_type_array = backend->get_uniform_location(pbr_shader, "uLightTypes");
        u_light_range_array = backend->get_uniform_location(pbr_shader, "uLightRanges");
        u_csm_splits = backend->get_uniform_location(pbr_shader, "uCsmSplits");
        u_csm_matrices = backend->get_uniform_location(pbr_shader, "uCsmMatrices");
        u_csm_cascade_count = backend->get_uniform_location(pbr_shader, "uCsmCascadeCount");

        // Emissive uniforms
        u_emissive_color = backend->get_uniform_location(pbr_shader, "uEmissiveColor");
        u_emissive_intensity = backend->get_uniform_location(pbr_shader, "uEmissiveIntensity");
        u_has_emissive_map = backend->get_uniform_location(pbr_shader, "uHasEmissiveMap");
        u_emissive_tex = backend->get_uniform_location(pbr_shader, "uEmissiveMap");

        // SH ambient uniforms
        u_sh_coeffs = backend->get_uniform_location(pbr_shader, "uShCoeffs");
        u_use_sh_ambient = backend->get_uniform_location(pbr_shader, "uUseShAmbient");

        // Reflection probe uniforms
        u_probe_count = backend->get_uniform_location(pbr_shader, "uProbeCount");
        u_probe_cubemaps = backend->get_uniform_location(pbr_shader, "uProbeCubemaps");
        u_probe_positions = backend->get_uniform_location(pbr_shader, "uProbePositions");
        u_probe_radius = backend->get_uniform_location(pbr_shader, "uProbeRadius");
        u_probe_intensity = backend->get_uniform_location(pbr_shader, "uProbeIntensity");

        // Color grading uniforms
        u_exposure = backend->get_uniform_location(pbr_shader, "uExposure");
        u_contrast = backend->get_uniform_location(pbr_shader, "uContrast");
        u_saturation = backend->get_uniform_location(pbr_shader, "uSaturation");
        u_brightness = backend->get_uniform_location(pbr_shader, "uBrightness");
        u_vignette_strength = backend->get_uniform_location(pbr_shader, "uVignetteStrength");
        u_vignette_radius = backend->get_uniform_location(pbr_shader, "uVignetteRadius");
        u_tonemap_mode = backend->get_uniform_location(pbr_shader, "uTonemapMode");

        // Fog uniforms
        u_height_fog_density = backend->get_uniform_location(pbr_shader, "uHeightFogDensity");
        u_height_fog_height = backend->get_uniform_location(pbr_shader, "uHeightFogHeight");
        u_height_fog_falloff = backend->get_uniform_location(pbr_shader, "uHeightFogFalloff");
        u_aerial_fog_density = backend->get_uniform_location(pbr_shader, "uAerialFogDensity");
        u_fog_color = backend->get_uniform_location(pbr_shader, "uFogColor");

        // SSAO uniforms
        u_ao_tex = backend->get_uniform_location(pbr_shader, "uAOTex");
        u_has_ao = backend->get_uniform_location(pbr_shader, "uHasAO");

        // TAA / motion vector uniforms
        u_prev_view = backend->get_uniform_location(pbr_shader, "uPrevView");
        u_prev_proj = backend->get_uniform_location(pbr_shader, "uPrevProj");

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

        // TAA jitter — produce a jittered projection for this frame
        float jitter_xy[2] = {};
        taa_.jitter_projection(proj, frame_index_, 1920, 1080, jittered_proj_, jitter_xy);
    }

    void submit(const PbrDrawCall& dc) {
        if (!pbr_shader || vao == 0) return;
        backend->use_shader(pbr_shader);
        glBindVertexArray(vao);

        // Per-frame uniforms — use jittered projection for TAA
        glUniformMatrix4fv(u_view, 1, GL_FALSE, view);
        glUniformMatrix4fv(u_projection, 1, GL_FALSE, jittered_proj_);

        // TAA motion vector uniforms (prev frame view/proj)
        glUniformMatrix4fv(u_prev_view, 1, GL_FALSE, taa_.prev_view());
        glUniformMatrix4fv(u_prev_proj, 1, GL_FALSE, taa_.prev_proj());
        glUniform3fv(u_view_pos, 1, cam_pos);
        glUniform1f(u_ambient, lights[0].ambient);
        glUniform3fv(u_ambient_sky, 1, ambient_sky_color_);
        glUniform3fv(u_ambient_ground, 1, ambient_ground_color_);

        // Multi-light uniforms — upload as flat arrays
        glUniform1i(u_light_count, light_count);
        float dir_data[kMaxLights * 3];
        float col_data[kMaxLights * 3];
        float pos_data[kMaxLights * 3];
        float inten_data[kMaxLights];
        int   type_data[kMaxLights];
        float range_data[kMaxLights];
        for (int i = 0; i < light_count; ++i) {
            dir_data[i*3+0] = lights[i].direction[0];
            dir_data[i*3+1] = lights[i].direction[1];
            dir_data[i*3+2] = lights[i].direction[2];
            col_data[i*3+0] = lights[i].color[0];
            col_data[i*3+1] = lights[i].color[1];
            col_data[i*3+2] = lights[i].color[2];
            pos_data[i*3+0] = lights[i].position[0];
            pos_data[i*3+1] = lights[i].position[1];
            pos_data[i*3+2] = lights[i].position[2];
            inten_data[i] = lights[i].intensity;
            type_data[i] = static_cast<int>(lights[i].type);
            range_data[i] = lights[i].range;
        }
        glUniform3fv(u_light_dir_array, light_count, dir_data);
        glUniform3fv(u_light_color_array, light_count, col_data);
        glUniform3fv(u_light_pos_array, light_count, pos_data);
        glUniform1fv(u_light_intensity, light_count, inten_data);
        glUniform1iv(u_light_type_array, light_count, type_data);
        glUniform1fv(u_light_range_array, light_count, range_data);

        // CSM uniforms
        glUniform1i(u_csm_cascade_count, kCsmCascadeCount);
        glUniform1fv(u_csm_splits, kCsmCascadeCount, cascade_splits);

        // Bind CSM shadow array
        if (shadow_pass && light_count > 0 && lights[0].cast_shadows) {
            shadow_pass->bind_shadow_map(3);  // bind entire CSM array
            glUniform1i(u_shadow_tex, 3);

            // Build CSM light-space matrices for each cascade
            float up[3] = {0, 1, 0};
            float fwd[3] = {lights[0].direction[0], lights[0].direction[1], lights[0].direction[2]};
            float len = std::sqrt(fwd[0]*fwd[0]+fwd[1]*fwd[1]+fwd[2]*fwd[2]);
            if (len > 0) { fwd[0]/=len; fwd[1]/=len; fwd[2]/=len; }
            float rt[3] = {up[1]*fwd[2]-up[2]*fwd[1], up[2]*fwd[0]-up[0]*fwd[2], up[0]*fwd[1]-up[1]*fwd[0]};
            len = std::sqrt(rt[0]*rt[0]+rt[1]*rt[1]+rt[2]*rt[2]);
            if (len > 0) { rt[0]/=len; rt[1]/=len; rt[2]/=len; }
            float up2[3] = {fwd[1]*rt[2]-fwd[2]*rt[1], fwd[2]*rt[0]-fwd[0]*rt[2], fwd[0]*rt[1]-fwd[1]*rt[0]};

            float csm_mats[4][16];
            for (int c = 0; c < kCsmCascadeCount; ++c) {
                // Cascade distance grows with each split
                float dist = 20.0F + cascade_splits[c] * 0.5F;
                float ortho_size = 15.0F + cascade_splits[c] * 0.5F;
                float eye[3] = {cam_pos[0]-fwd[0]*dist, cam_pos[1]-fwd[1]*dist, cam_pos[2]-fwd[2]*dist};
                // Light view matrix
                float lv[16];
                lv[0]=rt[0]; lv[1]=up2[0]; lv[2]=-fwd[0]; lv[3]=0;
                lv[4]=rt[1]; lv[5]=up2[1]; lv[6]=-fwd[1]; lv[7]=0;
                lv[8]=rt[2]; lv[9]=up2[2]; lv[10]=-fwd[2]; lv[11]=0;
                lv[12]=-(rt[0]*eye[0]+rt[1]*eye[1]+rt[2]*eye[2]);
                lv[13]=-(up2[0]*eye[0]+up2[1]*eye[1]+up2[2]*eye[2]);
                lv[14]=-(-fwd[0]*eye[0]-fwd[1]*eye[1]-fwd[2]*eye[2]);
                lv[15]=1;
                // Orthographic projection
                float lp[16] = {};
                float n=1.0F, f=200.0F, s=ortho_size;
                lp[0]=1/s; lp[5]=1/s; lp[10]=-2/(f-n); lp[11]=0;
                lp[14]=-(f+n)/(f-n); lp[15]=1;
                // Combine: light_space = projection * view
                for (int i=0;i<4;++i) for(int j=0;j<4;++j)
                    csm_mats[c][i*4+j]=lp[i*4+0]*lv[0*4+j]+lp[i*4+1]*lv[1*4+j]+lp[i*4+2]*lv[2*4+j]+lp[i*4+3]*lv[3*4+j];
            }
            glUniformMatrix4fv(u_csm_matrices, kCsmCascadeCount, GL_FALSE, &csm_mats[0][0]);
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
        bool has_ems = dc.emissive_map.id != 0;
        glUniform1i(u_has_albedo, has_alb ? 1 : 0);
        glUniform1i(u_has_normal, has_nrm ? 1 : 0);
        glUniform1i(u_has_orm, has_orm ? 1 : 0);
        glUniform1i(u_has_emissive_map, has_ems ? 1 : 0);

        if (has_alb) { backend->bind_texture(dc.albedo_map, 0); glUniform1i(u_albedo_tex, 0); }
        if (has_nrm) { backend->bind_texture(dc.normal_map, 1); glUniform1i(u_normal_tex, 1); }
        if (has_orm) { backend->bind_texture(dc.orm_map, 2); glUniform1i(u_orm_tex, 2); }
        if (has_ems) { backend->bind_texture(dc.emissive_map, 5); glUniform1i(u_emissive_tex, 5); }

        // AO texture
        bool has_ao = ao_texture_.id != 0;
        glUniform1i(u_has_ao, has_ao ? 1 : 0);
        if (has_ao) {
            backend->bind_texture(ao_texture_, 4);
            glUniform1i(u_ao_tex, 4);
        }

        // Emissive
        glUniform3fv(u_emissive_color, 1, dc.emissive_color);
        glUniform1f(u_emissive_intensity, dc.emissive_intensity);

        // SH ambient
        glUniform1i(u_use_sh_ambient, use_sh_ambient_ ? 1 : 0);
        if (use_sh_ambient_) {
            glUniform1fv(u_sh_coeffs, kShCoeffCount, sh_coeffs_);
        }

        // SSAO — bind AO texture if available and enabled
        if (ssao_enabled_ && ao_texture_.id != 0) {
            backend->bind_texture(ao_texture_, 4);
            glUniform1i(u_ao_tex, 4);
            glUniform1i(u_has_ao, 1);
        } else {
            glUniform1i(u_has_ao, 0);
        }

        // Color grading
        glUniform1f(u_exposure, color_grading_.exposure);
        glUniform1f(u_contrast, color_grading_.contrast);
        glUniform1f(u_saturation, color_grading_.saturation);
        glUniform1f(u_brightness, color_grading_.brightness);
        glUniform1f(u_vignette_strength, color_grading_.vignette_strength);
        glUniform1f(u_vignette_radius, color_grading_.vignette_radius);
        glUniform1i(u_tonemap_mode, color_grading_.tonemap_mode);

        // Fog
        glUniform1f(u_height_fog_density, fog_params_.height_fog_density);
        glUniform1f(u_height_fog_height, fog_params_.height_fog_height);
        glUniform1f(u_height_fog_falloff, fog_params_.height_fog_falloff);
        glUniform1f(u_aerial_fog_density, fog_params_.aerial_fog_density);
        glUniform3fv(u_fog_color, 1, fog_params_.fog_color);

        // Reflection probes
        glUniform1i(u_probe_count, reflection_probe_count_);
        if (reflection_probe_count_ > 0) {
            float probe_pos_data[kMaxReflectionProbes * 3];
            float probe_rad_data[kMaxReflectionProbes];
            float probe_int_data[kMaxReflectionProbes];
            for (int i = 0; i < reflection_probe_count_; ++i) {
                probe_pos_data[i*3+0] = reflection_probes_[i].position[0];
                probe_pos_data[i*3+1] = reflection_probes_[i].position[1];
                probe_pos_data[i*3+2] = reflection_probes_[i].position[2];
                probe_rad_data[i] = reflection_probes_[i].influence_radius;
                probe_int_data[i] = reflection_probes_[i].intensity;
                // Bind probe cubemap — slot 6+i
                if (reflection_probes_[i].cubemap.id != 0) {
                    glActiveTexture(GL_TEXTURE6 + i);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, reflection_probes_[i].cubemap.id);
                    glUniform1i(u_probe_cubemaps + i, 6 + i);
                }
            }
            glUniform3fv(u_probe_positions, reflection_probe_count_, probe_pos_data);
            glUniform1fv(u_probe_radius, reflection_probe_count_, probe_rad_data);
            glUniform1fv(u_probe_intensity, reflection_probe_count_, probe_int_data);
        }

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
        ssao_pass_.shutdown();
        taa_.shutdown();
        if (vao != 0) {
            glDeleteVertexArrays(1, &vao);
            vao = 0;
        }
        if (pbr_shader) {
            backend->destroy_shader(pbr_shader);
            pbr_shader = {};
        }
    }

    void end_frame() {
        // Store current view/proj as previous for next frame's motion vectors
        taa_.store_prev_matrices(view, proj);
        ++frame_index_;
    }

    void set_ssao_enabled(bool enabled) {
        ssao_enabled_ = enabled;
    }

    void set_frame_index(int index) {
        frame_index_ = index;
    }

    const float* jittered_projection() const {
        return jittered_proj_;
    }
};

PbrRenderer::PbrRenderer() : impl_(std::make_unique<Impl>()) {}
PbrRenderer::~PbrRenderer() = default;
bool PbrRenderer::initialize(RenderBackend* backend) { return impl_->initialize(backend); }
void PbrRenderer::shutdown() { impl_->shutdown(); }
void PbrRenderer::set_lights(const PbrLight* lights, int count) { impl_->set_lights(lights, count); }
void PbrRenderer::set_cascade_splits(const float* splits) { impl_->set_cascade_splits(splits); }
void PbrRenderer::set_ambient_sh(const float* coeffs) { impl_->set_ambient_sh(coeffs); }
void PbrRenderer::set_ambient_sky_ground(const float sky[3], const float ground[3]) {
    impl_->set_ambient_sky_ground(sky, ground);
}
void PbrRenderer::set_reflection_probes(const ReflectionProbe* probes, int count) { impl_->set_reflection_probes(probes, count); }
void PbrRenderer::set_ao_texture(TextureHandle tex) {
    impl_->set_ao_texture(tex);
}
void PbrRenderer::set_color_grading(const ColorGradingParams& params) { impl_->set_color_grading(params); }
void PbrRenderer::set_fog(const FogParams& params) { impl_->set_fog(params); }
void PbrRenderer::set_ssao_enabled(bool enabled) {
    impl_->set_ssao_enabled(enabled);
}
void PbrRenderer::set_frame_index(int index) {
    impl_->set_frame_index(index);
}
void PbrRenderer::begin_frame(const float* v, const float* p, const float* c, ShadowPass* s) { impl_->begin_frame(v, p, c, s); }
void PbrRenderer::submit(const PbrDrawCall& dc) { impl_->submit(dc); }
const float* PbrRenderer::jittered_projection() const {
    return impl_->jittered_projection();
}
void PbrRenderer::end_frame() { impl_->end_frame(); }

} // namespace ae::render
