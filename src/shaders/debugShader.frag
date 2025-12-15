#version 450

layout(set = 0, binding = 0) uniform PassData {
    mat4 lightViewProjection;
    vec3 cameraPosition;
    float padding1;
    vec3 lightDirection;
    float padding2;
    vec3 lightColor;
    float padding3;
};

layout(location = 0) in vec4 color;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 fragPosition;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 cameraDir = normalize(cameraPosition - fragPosition);

    float diffuse = dot(normal, -lightDirection);
    diffuse = max(diffuse, 0.2);

    float f = dot(cameraDir, normal);
    float fresnel = mix(0.8, 1.1, pow(f, 4));

    vec4 shaded = vec4(vec3(color) * diffuse * fresnel, color.a);
    shaded.xyz = shaded.xyz / (shaded.xyz + 0.5);
    outColor = shaded;
}
