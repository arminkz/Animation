#version 450

// Per-frame variables (set 0 is per-frame descriptor set)
layout(set = 0, binding = 0) uniform SceneInfo {
    mat4 view;
    mat4 proj;

    float time;
    vec3 cameraPosition;
    vec3 lightColor;
} si;

// Per-model variabales that change a lot 
layout(push_constant) uniform PushConstants {
    mat4 model;
    uint objectId;
} pc;

layout(location = 0) in vec3 inPosition; // Vertex position
layout(location = 1) in vec4 inColor;    // Vertex color
layout(location = 2) in vec2 inTexCoord; // Vertex texture coordinate
layout(location = 3) in vec3 inNormal;   // Vertex normal
layout(location = 4) in vec3 inTangent;  // Vertex tangent

// Passed from the vertex shader to the fragment shader
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec4 worldPosition;
layout(location = 3) out vec3 worldNormal;
layout(location = 4) out vec3 worldTangent;
layout(location = 5) out vec3 positionView;
layout(location = 6) out vec3 normalView;

void main() {
    worldPosition = pc.model * vec4(inPosition, 1.0);
    worldNormal = (pc.model * vec4(inNormal, 0.0)).xyz;
    worldTangent = (pc.model * vec4(inTangent, 0.0)).xyz;

    mat3 normalMatrix = transpose(inverse(mat3(si.view * pc.model)));
    normalView = normalize(normalMatrix * inNormal);
    positionView = normalize((si.view * worldPosition).xyz);

    gl_Position = si.proj * si.view * worldPosition;
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}