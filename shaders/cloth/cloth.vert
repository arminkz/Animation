#version 450

layout(set = 0, binding = 0) uniform SceneInfo {
    mat4 view;
    mat4 proj;
    float time;
    vec3 cameraPosition;
    vec3 lightColor;
} si;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 worldNormal;
layout(location = 1) out vec3 worldPosition;
layout(location = 2) out vec2 fragTexCoord;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    worldPosition = worldPos.xyz;
    worldNormal   = normalize((pc.model * vec4(inNormal, 0.0)).xyz);
    fragTexCoord  = inTexCoord;

    gl_Position = si.proj * si.view * worldPos;
}
