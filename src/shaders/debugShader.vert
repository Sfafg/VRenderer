#version 460
#include "DrawCall.glsl"

struct ObjectData {
    mat4 model;
    vec4 color;
};

layout(push_constant) uniform PushConstants {
    mat4 cameraViewProjection;
    uint drawIdOffset;
};

layout(set = 0, binding = 0) uniform PassData {
    mat4 lightViewProjection;
    vec3 cameraPosition;
    float padding1;
    vec3 lightDirection;
    float padding2;
    vec3 lightColor;
    float padding3;
};

layout(set = 0, binding = 2) readonly buffer ObjectDatas { ObjectData objectData[]; };

layout(set = 0, binding = 3) readonly buffer DrawCalls { DrawCall drawCalls[]; };

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in uint objectIndex;

layout(location = 0) out vec4 color;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec3 fragPosition;

void main() {
    // DrawCall drawCall = drawCalls[gl_DrawID + drawIdOffset];
    DrawCall drawCall = drawCalls[drawIdOffset];
    ObjectData data = objectData[objectIndex];

    gl_Position = cameraViewProjection * data.model * vec4(aPosition, 1);
    color = data.color;
    normal = normalize(mat3(transpose(inverse(data.model))) * aNormal);
    fragPosition = vec3(data.model * vec4(aPosition, 1.0));
}
