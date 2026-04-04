#version 450

layout(set = 0, binding = 0) uniform SceneInfo {
    mat4 view;
    mat4 proj;
    float time;
    vec3 cameraPosition;
    vec3 lightColor;
} si;

layout(location = 0) in  vec3 worldNormal;
layout(location = 1) in  vec3 worldPosition;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(worldNormal);

    // Key light from above-right
    vec3 lightDir = normalize(vec3(1.0, 2.0, 1.0));
    float diffuse  = max(dot(N, lightDir), 0.0);

    // Rim light from below-left
    vec3 rimDir   = normalize(vec3(-1.0, -0.5, -1.0));
    float rim     = pow(max(dot(N, rimDir), 0.0), 3.0) * 0.15;

    float ambient = 0.15;
    float light   = ambient + diffuse * 0.75 + rim;

    vec3 baseColor = vec3(0.72, 0.72, 0.72); // neutral grey
    outColor = vec4(baseColor * light, 1.0);
}
