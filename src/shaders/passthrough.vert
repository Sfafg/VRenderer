#version 450

layout(push_constant) uniform PushConstants{
    mat4 viewProjection;
};

layout(std140, binding = 0) buffer InstanceData{
    mat4 model[];
};

layout(location = 0) in vec3 aPosition;
layout(location = 1) in uint aObjectID;

void main() {
    gl_Position = viewProjection * model[aObjectID] * vec4(aPosition, 1.0);
}