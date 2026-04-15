#version 450

layout(set = 0, binding = 0) uniform SceneInfo {
    mat4 view;
    mat4 proj;
    float time;
    vec3 cameraPosition;
    vec3 lightColor;
} si;

layout(set = 1, binding = 0) uniform sampler2D flagTexture;

layout(location = 0) in vec3 worldNormal;
layout(location = 1) in vec3 worldPosition;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 lightDir = normalize(vec3(1.0, 2.0, 1.0));
    vec3 N = normalize(worldNormal);

    // Two-sided diffuse
    float diffuse = abs(dot(N, lightDir));
    float ambient = 0.2;
    float light   = ambient + diffuse * 0.8;

    vec3 texColor = texture(flagTexture, fragTexCoord).rgb;
    outColor = vec4(texColor * light, 1.0);
}
