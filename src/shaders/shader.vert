#version 450

struct DrawCall 
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    uint vertexOffset;
    uint firstInstance;
    uint materialIndex ;
    uint meshIndex ;
    uint batchDataElementSize;
    uint lodCountPointerParent;
    uint firstObject;
};

layout(set = 0, binding = 0) uniform PassData{
    mat4 viewProjection;
};

layout(set = 0, binding = 1) readonly buffer MaterialData {
    vec4 color[];
};

layout(set=0, binding = 2) readonly buffer ObjectData{mat4 objectData[];}; 

layout(set=0, binding = 3) readonly buffer DrawCalls{DrawCall drawCalls[];}; 

layout(location = 0) in vec2 aPosition;
layout(location = 1) in uint objectIndex;
layout(location = 2) in uint batchIndex;

layout(location = 0) out vec4 col;

void main() {
    DrawCall drawCall = drawCalls[batchIndex];

    gl_Position = viewProjection*objectData[objectIndex + drawCall.firstObject]*vec4(aPosition.x, aPosition.y,0 ,1);
    col = vec4(color[drawCall.materialIndex]);
}
