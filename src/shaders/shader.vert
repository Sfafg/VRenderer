#version 450

layout(set = 0, binding = 0) uniform PassData{
    mat4 viewProjection;
};

layout(push_constant) uniform PushConstants {
    int materialOffset;
};

layout(set = 0, binding = 1) readonly buffer MaterialData {
    float color[];
};

layout(location = 0) in vec2 aPosition;
layout(location = 1) in mat4 model;

layout(location = 0) out vec4 col;

void main() {
    gl_Position = viewProjection*model*vec4(aPosition.x, aPosition.y,0 ,1);
    col = vec4(color[materialOffset]);
}
