#version 450
#include "DrawCall.glsl"

layout(set = 0, binding = 0) uniform PassData {
    mat4 cameraViewProjection;
    mat4 lightViewProjection;
    vec3 cameraPosition;
    float padding1;
    vec3 lightDirection;
    float padding2;
    vec3 lightColor;
    float padding3;
};

struct Material {
    vec4 color;
    float roughness;
};
layout(set = 0, binding = 1) readonly buffer MaterialData { Material materials[]; };

layout(set = 0, binding = 2) readonly buffer ObjectData { mat4 objectData[]; };

layout(set = 0, binding = 3) readonly buffer DrawCalls { DrawCall drawCalls[]; };

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in uint objectIndex;
layout(location = 3) in uint batchIndex;

layout(location = 0) out vec4 color;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec3 fragPosition;
layout(location = 3) out vec4 fragLightPosition;
layout(location = 4) out float roughness;

void main() {
    DrawCall drawCall = drawCalls[batchIndex];
    mat4 model = objectData[objectIndex + drawCall.firstObject];
    Material material = materials[drawCall.materialIndex];

    gl_Position = cameraViewProjection * model * vec4(aPosition, 1);
    color = vec4(material.color);
    normal = normalize(mat3(transpose(inverse(model))) * aNormal);
    roughness = material.roughness;
    fragPosition = vec3(model * vec4(aPosition, 1));
    fragLightPosition = lightViewProjection * model * vec4(aPosition, 1);
}
