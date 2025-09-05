#version 450

struct DrawCall {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    uint vertexOffset;
    uint firstInstance;
    uint materialIndex;
    uint meshIndex;
    uint batchDataElementSize;
    uint lodCountPointerParent;
    uint firstObject;
};

layout(set = 0, binding = 1) readonly buffer MaterialData { vec4 offset[]; };

layout(set = 0, binding = 2) readonly buffer ObjectData { float objectData[]; };

layout(set = 0, binding = 3) readonly buffer DrawCalls { DrawCall drawCalls[]; };

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec3 aCol;
layout(location = 2) in uint objectIndex;
layout(location = 3) in uint batchIndex;

layout(location = 0) out vec3 col;

void main() {
    DrawCall drawCall = drawCalls[batchIndex];

    vec4 off = offset[drawCall.materialIndex];
    gl_Position = vec4(aPosition.x * off.z + off.x, aPosition.y * off.w + off.y, 0, 1);
    col = aCol * objectData[drawCall.firstObject + objectIndex];
}
