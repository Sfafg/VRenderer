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
};

layout(set = 0, binding = 0) uniform PassData{
    mat4 viewProjection;
};

layout(set = 0, binding = 1) readonly buffer MaterialData {
    float color[];
};

layout(set=0, binding = 2) readonly buffer ObjectData{mat4 objectData[];}; 

layout(set=0, binding = 3) readonly buffer DrawCalls{DrawCall drawCalls[];}; 

// batch data buffer
// material buffer
// object data buffer

layout(location = 0) in vec2 aPosition;
layout(location = 1) in uint objectIndex;

layout(location = 0) out vec4 col;

void main() {

    DrawCall drawCall;
    for(int i = 1; i >= 0; i--)
        if(objectIndex >= drawCalls[i].firstInstance){
            drawCall= drawCalls[i] ;
            break;
        }

    gl_Position = viewProjection*objectData[objectIndex]*vec4(aPosition.x, aPosition.y,0 ,1);
    col = vec4(color[drawCall.materialIndex]);
}
