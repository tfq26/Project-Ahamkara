#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec4 vLightSpacePos;
in vec2 vTexCoord;

uniform sampler2D uAlbedoMap;
uniform sampler2D uNormalMap;
uniform sampler2D uOrmMap;
uniform sampler2D uShadowMap;
uniform sampler2D uEmissiveMap;
uniform sampler2D uAOTexture;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uViewPos;
uniform vec3 uAlbedo;
uniform vec3 uEmissiveColor;
uniform vec3 uAmbientSkyColor;
uniform vec3 uAmbientGroundColor;
uniform float uMetallic;
uniform float uRoughness;
uniform float uAmbientStrength;
uniform float uEmissiveIntensity;
uniform bool uHasAlbedoMap;
uniform bool uHasNormalMap;
uniform bool uHasOrmMap;
uniform bool uHasEmissiveMap;
uniform bool uHasAOTexture;

// SSAO uniforms
uniform sampler2D uAOTex;
uniform bool uHasAO;

// Color grading uniforms
uniform float uExposure, uContrast, uSaturation, uBrightness;
uniform float uVignetteStrength, uVignetteRadius;
uniform int uTonemapMode;

out vec4 fragColor;

const float PI = 3.14159265359;

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return geometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
           geometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float shadowCalculation(vec4 lightSpacePos) {
    vec3 proj = lightSpacePos.xyz / lightSpacePos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0) return 0.0;

    float currentDepth = proj.z;
    float bias = 0.005;
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);

    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(uShadowMap, proj.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }

    return shadow / 9.0;
}

void main() {
    vec3 albedo = uHasAlbedoMap ? texture(uAlbedoMap, vTexCoord).rgb : uAlbedo;
    float metallic = uHasOrmMap ? texture(uOrmMap, vTexCoord).b : uMetallic;
    float roughness = uHasOrmMap ? texture(uOrmMap, vTexCoord).g : uRoughness;
    roughness = clamp(roughness, 0.04, 1.0);

    vec3 N = normalize(vNormal);
    if (uHasNormalMap) {
        vec3 tangentNormal = texture(uNormalMap, vTexCoord).rgb * 2.0 - 1.0;
        N = normalize(N + tangentNormal * 0.5);
    }

    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 L = normalize(uLightDir);
    vec3 H = normalize(V + L);
    vec3 R = reflect(-V, N);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float NDF = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kD = (1.0 - F) * (1.0 - metallic);
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = kD * albedo / PI;

    // ── Ambient (hemispherical) ──
    float hemisphere = 0.5 + 0.5 * N.y;
    vec3 ambient_color = mix(uAmbientGroundColor, uAmbientSkyColor, hemisphere);
    float occlusion = 1.0;
    if (uHasAOTexture) {
        occlusion = texture(uAOTexture, vTexCoord).r;
    }
    vec3 ambient = ambient_color * uAmbientStrength * albedo * occlusion;

    float shadow = shadowCalculation(vLightSpacePos);
    vec3 lighting = ambient + (1.0 - shadow) * (diffuse + specular) * uLightColor * NdotL;

    // ── Emissive ──
    vec3 emissive = uEmissiveColor * uEmissiveIntensity;
    if (uHasEmissiveMap) {
        emissive += texture(uEmissiveMap, vTexCoord).rgb * uEmissiveIntensity;
    }
    lighting += emissive;

    // ── SSAO factor ──
    float ao_factor = 1.0;
    if (uHasAO) {
        ao_factor = 1.0 - texture(uAOTex, vTexCoord).r;
        ao_factor = clamp(ao_factor, 0.0, 1.0);
    }
    lighting *= ao_factor;

    // ── Color grading ──
    lighting *= uExposure;
    lighting = (lighting - 0.5) * uContrast + 0.5;
    float lum = dot(lighting, vec3(0.299, 0.587, 0.114));
    lighting = mix(vec3(lum), lighting, uSaturation);
    lighting += uBrightness;

    if (uTonemapMode == 1) {
        lighting = lighting / (lighting + vec3(1.0));
    } else if (uTonemapMode == 2) {
        vec3 a = lighting * (lighting + 0.0245786) - 0.000090537;
        vec3 b = lighting * (0.983729 * lighting + 0.4329510) + 0.238081;
        lighting = a / b;
    }

    vec2 uv_center = vTexCoord - 0.5;
    float vignette = 1.0 - dot(uv_center, uv_center) * uVignetteStrength;
    vignette = clamp(vignette, 0.0, 1.0);
    lighting *= smoothstep(uVignetteRadius, 1.0, vignette);

    fragColor = vec4(lighting, 1.0);
}
