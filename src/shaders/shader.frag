#version 450

layout(set = 0, binding = 0) uniform LightData {
    mat4 lightViewProjection;
    vec3 lightDirection;
    float padding1;
    vec3 lightColor;
    float padding2;
};

layout(set = 0, binding = 4) uniform sampler2D shadowMap;

layout(location = 0) in vec4 color;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 fragPosition;
layout(location = 3) in vec4 fragLightPosition;
layout(location = 4) in float roughness;
layout(location = 5) in vec3 cameraPosition;

layout(location = 0) out vec4 outColor;

vec3 linear_to_AgX(vec3 lin) {
    // Hable-like filmic curve
    float A = 0.15; // shoulder strength
    float B = 0.50; // shoulder curvature
    float C = 0.10; // midtone contrast
    float D = 0.20; // linear section
    float E = 0.02; // toe strength
    float F = 0.30; // toe curvature

    // Exposure
    vec3 x = lin * 0.7;

    // Curve function
    vec3 numerator = x * (A * x + C * B) + D * E;
    vec3 denominator = x * (A * x + B) + D * F;
    vec3 y = (numerator / denominator) - E / F;

    // White point (normalize so W maps to 1.0)
    float W = 11.2; // ≈ linear value for "white"
    float whiteScale = ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F;
    y /= whiteScale;

    // Gamma to sRGB
    y = pow(clamp(y, 0.0, 1.0), vec3(1.0 / 2.2));

    return y;
}

uint hash1D(uint x) {
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

float noise1D(uint x) { return hash1D(x) / 4294967295.0 * 2 - 1; }
vec2 noise2D(uvec2 x) { return vec2(noise1D(x.x), noise1D(x.y)); }

// float ShadowDepth(vec3 lightLocalFragPosition, vec2 offset) {
//     float shadowDepth = texture(shadowMap, lightLocalFragPosition.xy * 0.5 + 0.5 + offset).r;
//     float lightDistance = lightLocalFragPosition.z - lightDepth;
//     if (lightDistance < 0) shadowDepth = 0;
//     return shadowDistance;
// }

const vec2 poissonDisk[32] = vec2[](
    vec2(-0.940, -0.399), vec2(0.945, -0.768), vec2(-0.094, 0.929), vec2(0.345, 0.293), vec2(-0.915, 0.457),
    vec2(0.505, -0.068), vec2(-0.565, -0.859), vec2(0.575, 0.879), vec2(-0.183, 0.289), vec2(0.812, 0.397),
    vec2(-0.481, 0.642), vec2(0.066, -0.509), vec2(-0.705, -0.159), vec2(0.240, -0.924), vec2(0.156, 0.724),
    vec2(-0.337, -0.141), vec2(0.710, -0.537), vec2(-0.783, 0.779), vec2(0.423, 0.728), vec2(-0.059, -0.846),
    vec2(-0.339, 0.930), vec2(0.921, 0.038), vec2(-0.831, -0.638), vec2(0.729, 0.693), vec2(-0.691, 0.219),
    vec2(0.004, 0.059), vec2(0.285, -0.642), vec2(-0.236, 0.531), vec2(0.614, -0.315), vec2(-0.470, -0.504),
    vec2(0.390, -0.383), vec2(-0.027, 0.374)
);

float SampleShadowDepth(vec2 lightLocalFragPosition, vec2 offset) {
    return texture(shadowMap, lightLocalFragPosition.xy * 0.5 + 0.5 + offset).r;
}

float EstimatePenumbraWidth() {
    int SAMPLE_COUNT = 20;
    float LIGHT_WIDTH = 10;
    float receiverDepth = fragLightPosition.z;
    float blockerDepth = 0;
    float blockerPointCount = 0;
    float searchRadius = LIGHT_WIDTH / receiverDepth / 8192.0;

    float angle = noise1D(hash1D(int(fragPosition.x * 7919.3)) + hash1D(int(fragPosition.y * 7919.3))) * 3.14;
    mat2 rotationMatrix = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        vec2 coords = rotationMatrix * poissonDisk[i];
        float candidateDepth = SampleShadowDepth(fragLightPosition.xy, coords * searchRadius);
        if (candidateDepth < receiverDepth) {
            blockerDepth += candidateDepth;
            blockerPointCount++;
        }
    }
    if (blockerPointCount == 0) return 0;
    blockerDepth /= blockerPointCount;

    return (receiverDepth - blockerDepth) * LIGHT_WIDTH / blockerDepth * 2;
}

float PCF() {
    int SAMPLE_COUNT = 20;
    float penumbraWidth = EstimatePenumbraWidth() + 0.15;
    float receiverDepth = fragLightPosition.z;
    int blockedPointCount = 0;

    float angle = noise1D(hash1D(int(fragPosition.x * 7919.3)) + hash1D(int(fragPosition.y * 7919.3))) * 3.14;
    mat2 rotationMatrix = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        vec2 coords = rotationMatrix * poissonDisk[i];
        float candidateDepth = SampleShadowDepth(fragLightPosition.xy, coords / 8192.0 * penumbraWidth);
        if (candidateDepth < receiverDepth) blockedPointCount++;
    }

    return 1.0 - blockedPointCount / float(SAMPLE_COUNT);
}

void main() {
    float specularStrength = 0.5;
    float ambientStrength = 0.1;
    vec3 ambient = lightColor * ambientStrength;

    float diff = max(dot(normal, -lightDirection), 0.0);
    vec3 diffuse = diff * lightColor;

    float shininess = mix(1.0, 128.0, 1 - roughness);
    vec3 viewDir = normalize(cameraPosition - fragPosition);
    vec3 halfDir = normalize(-lightDirection + viewDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * lightColor;

    float lightPercentage = PCF();

    vec3 shaded = (ambient + diffuse * lightPercentage) * color.rgb + specular * lightPercentage;

    shaded = linear_to_AgX(shaded);
    outColor = vec4(shaded, color.a);
}
