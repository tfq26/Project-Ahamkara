#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aJointIndices;
layout(location = 3) in vec4 aJointWeights;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightSpace;
uniform mat3 uNormalMatrix;
uniform bool uHasSkinning;
uniform mat4 uJointMatrices[64];

out vec3 vWorldPos;
out vec3 vNormal;
out vec4 vLightSpacePos;
out vec2 vTexCoord;

void main() {
    mat4 model = uModel;
    vec4 worldPos;
    vec3 normal;

    if (uHasSkinning) {
        mat4 skin = 
            aJointWeights.x * uJointMatrices[int(aJointIndices.x)] +
            aJointWeights.y * uJointMatrices[int(aJointIndices.y)] +
            aJointWeights.z * uJointMatrices[int(aJointIndices.z)] +
            aJointWeights.w * uJointMatrices[int(aJointIndices.w)];
        worldPos = model * skin * vec4(aPosition, 1.0);
        normal = mat3(model) * mat3(skin) * aNormal;
    } else {
        worldPos = model * vec4(aPosition, 1.0);
        normal = mat3(model) * aNormal;
    }

    gl_Position = uProjection * uView * worldPos;
    vWorldPos = worldPos.xyz;
    vNormal = normalize(normal);
    vLightSpacePos = uLightSpace * worldPos;
    vTexCoord = vec2(0.0);
}
