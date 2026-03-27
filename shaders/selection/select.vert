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

void main() {
    gl_Position = si.proj * si.view * pc.model * vec4(inPosition, 1.0);
}