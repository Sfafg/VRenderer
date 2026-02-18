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

float noise1D(float x) { return hash1D(uint(x * 4294967295.0)) / 4294967295.0 * 2 - 1; }
vec2 noise2D(vec2 x) { return vec2(noise1D(x.x), noise1D(x.y)); }

float ShadowDepth(vec3 lightLocalFragPosition, vec2 offset) {
    float lightDepth = texture(shadowMap, lightLocalFragPosition.xy * 0.5 + 0.5 + offset).r;
    float shadowDistance = lightLocalFragPosition.z - lightDepth;
    if (shadowDistance < 0) shadowDistance = 0;
    return shadowDistance;
}

void main() {
    float specularStrength = 0.5;

    float ambientStrength = 0.1;
    // float ambientFactor = max(0, dot(normal, vec3(0, 0, 1)));
    // vec3 groundColor = vec3(0.5, 0.3, 0);
    // vec3 skyColor = vec3(0.2, 0.7, 1);
    vec3 ambient = lightColor * ambientStrength; // mix(groundColor, skyColor, ambientFactor) *

    float diff = max(dot(normal, -lightDirection), 0.0);
    vec3 diffuse = diff * lightColor;

    float shininess = mix(1.0, 128.0, 1 - roughness);
    vec3 viewDir = normalize(cameraPosition - fragPosition);
    vec3 halfDir = normalize(-lightDirection + viewDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * lightColor;

    vec3 lightLocalFragPos = fragLightPosition.xyz / fragLightPosition.w;
    const int resolution = 2;
    float shadowDepth = 0;
    int inShadow = 0;
    for (int i = -resolution; i <= resolution; i++) {
        for (int j = -resolution; j <= resolution; j++) {
            float d = ShadowDepth(lightLocalFragPos, vec2(i * 8, j * 8) / resolution / 1024);
            if (d != 0) inShadow++;
            shadowDepth += d;
        }
    }
    shadowDepth /= inShadow;
    shadowDepth = max(shadowDepth, 0.01);

    float lightPercentage = 0;
    for (int i = -resolution; i <= resolution; i++) {
        for (int j = -resolution; j < resolution; j++) {
            vec2 noise = noise2D(lightLocalFragPos.xy) * 0.0004;
            vec2 offset = (vec2(i, j) / resolution / 1024 + noise) * shadowDepth * 20;
            if (ShadowDepth(lightLocalFragPos, offset) == 0) lightPercentage++;
        }
    }
    lightPercentage /= pow(resolution * 2 + 1, 2);
    vec3 shaded = (ambient + diffuse * lightPercentage) * color.rgb + specular * lightPercentage;

    shaded = linear_to_AgX(shaded);
    outColor = vec4(shaded, color.a);
}
