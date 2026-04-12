#version 450

// Per-frame scene UBO (set=0)
layout(set = 0, binding = 0) uniform SceneInfo {
    mat4 view;
    mat4 proj;

    float time;
    vec3 cameraPosition;
    vec3 lightColor;
} si;

// Per-instance transforms written by boid_simulate.comp (set=1)
layout(set = 1, binding = 0) readonly buffer Transforms {
    mat4 transforms[];
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec3 inTangent;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec4 worldPosition;
layout(location = 3) out vec3 worldNormal;
layout(location = 4) out vec3 worldTangent;

void main() {
    // Instance rendering: one draw call for all boids, with per-boid transforms from the compute shader
    mat4 model = transforms[gl_InstanceIndex]; 

    worldPosition =  model * vec4(inPosition, 1.0);
    worldNormal   = (model * vec4(inNormal,  0.0)).xyz;
    worldTangent  = (model * vec4(inTangent, 0.0)).xyz;

    gl_Position = si.proj * si.view * worldPosition;
    fragColor   = inColor;
    fragTexCoord = inTexCoord;
}
