#version 450

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

layout(set = 0, binding = 4) uniform sampler2D shadowMap;

layout(location = 0) in vec4 color;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 fragPosition;
layout(location = 3) in vec4 fragLightPosition;
layout(location = 4) in float roughness;

layout(location = 0) out vec4 outColor;

void main() {
    float specularStrength = 0.5;

    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    float diff = max(dot(normal, -lightDirection), 0.0);
    vec3 diffuse = diff * lightColor;

    float shininess = mix(1.0, 128.0, 1 - roughness);
    vec3 viewDir = normalize(cameraPosition - fragPosition);
    vec3 halfDir = normalize(-lightDirection + viewDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * lightColor;

    vec3 fragLPos = fragLightPosition.xyz / fragLightPosition.w;
    // fragLPos.y *= -1;
    float d = fragLPos.z;
    float shadow = texture(shadowMap, fragLPos.xy * 0.5 + 0.5).r;
    if (shadow >= d) shadow = 1;
    else shadow = 0;

    vec4 shaded = vec4(ambient + diffuse * shadow, 1) * color + vec4(specular, 1) * shadow;

    outColor = shaded;
}
