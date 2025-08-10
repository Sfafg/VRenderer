#version 450

layout(push_constant) uniform PushConstants {
    int materialOffset;
};

layout(set = 0, binding = 1) buffer MaterialData {
    vec4 offset[];
};

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec3 aCol;

layout(location = 0) out vec3 col;

void main() {
    vec4 off = offset[materialOffset];
    gl_Position = vec4(aPosition.x*off.z+off.x, aPosition.y*off.w+off.y, 0,1);
    col = aCol;
}
