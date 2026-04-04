#version 450

layout(set = 0, binding = 0) uniform SceneInfo {
    mat4 view;
    mat4 proj;
    float time;
    vec3 cameraPosition;
    vec3 lightColor;
} si;

layout(location = 0) in  vec3 worldPosition;
layout(location = 0) out vec4 outColor;

void main() {
    // Flat normal from position derivatives — correct at all edges/corners, no vertex normal needed
    vec3 N = normalize(cross(dFdx(worldPosition), dFdy(worldPosition)));
    vec3 lightDir = normalize(vec3(1.0, 2.0, 1.0));

    // Two-sided diffuse
    float diffuse = abs(dot(N, lightDir));

    // Simulated subsurface scattering: back-transmitted light
    //float sss = pow(max(-dot(N, lightDir), 0.0), 2.0) * 0.5;

    // Fresnel-like rim
    vec3 viewDir = normalize(si.cameraPosition - worldPosition);
    float rim    = pow(1.0 - abs(dot(N, viewDir)), 3.0) * 0.4;

    float light = 0.15 + diffuse * 0.6; //+ sss; //+ rim;

    vec3 jellyColor = vec3(0.18, 0.72, 0.52);
    outColor = vec4(jellyColor * light, 1.0);
}
