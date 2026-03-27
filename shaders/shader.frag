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

// Per-model variables that don't change a lot
layout(set = 1, binding = 0) uniform materialUBO {  // Material information (set 1 is per-model descriptor set) 
    int hasBaseColorTexture;
    int hasUnlitColorTexture;
    int hasNormalMapTexture;
    int hasSpecularTexture;
    int hasOverlayColorTexture;

    float ambientStrength;
    float specularStrength;
    float overlayOffset;

    int sunShadeMode;
} material;

layout(set = 1, binding = 1) uniform sampler2D baseColorTexture;
layout(set = 1, binding = 2) uniform sampler2D unlitColorTexture;
layout(set = 1, binding = 3) uniform sampler2D normalMapTexture;
layout(set = 1, binding = 4) uniform sampler2D specularTexture;
layout(set = 1, binding = 5) uniform sampler2D overlayColorTexture;

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec4 worldPosition;
layout(location = 3) in vec3 worldNormal;
layout(location = 4) in vec3 worldTangent;
layout(location = 5) in vec3 worldViewPosition;
layout(location = 6) in vec3 normalView;

layout(location = 0) out vec4 outColor;

const mat2 rot = mat2( 1.6,  1.2, -1.2,  1.6 );

vec2 distortUV(vec2 uv, float time) {
    // Sample noise texture to get offset values
    float noise = texture(overlayColorTexture, uv * 3.0 + time * 0.1).r;
    float noise2 = texture(overlayColorTexture, rot * uv * 3.0 + time * 0.1).r;
    vec2 offset = vec2(noise * 0.01, noise2 * 0.01); // distortion amount
    return uv + offset;
}

void main() {
    vec3 N = normalize(worldNormal);
    vec3 T = normalize(worldTangent);
    // fragment tangent: after interpolation it might no longer be exactly perpendicular to N.
    // Perform Gram-Schmidt
    T = normalize(T - dot(T, N) * N);
    // Bi-normal
    vec3 B = normalize(cross(N, T));
    mat3 TBN_to_world = mat3(T, B, N);
    mat3 world_to_TBN = transpose(TBN_to_world); // Because its an orthonormal matrix, its inverse is just transpose.

    vec3 center = pc.model[3].xyz;

    vec3 lightPosition = vec3(0.0, 0.0, 0.0);
    vec3 lightDir = normalize(lightPosition - center); // Calculate light direction in world space
    vec3 lightDirTBN = world_to_TBN * lightDir;

    vec3 normalTBN = vec3(0.0, 0.0, 1.0);
    float normalDot = dot(normalTBN, lightDirTBN);
    if (material.hasNormalMapTexture == 1) {
        normalTBN = texture(normalMapTexture, fragTexCoord).rgb * 2.0 - 1.0;
    }
    float diffuseDot = dot(normalTBN, lightDirTBN);
    
    float mixAmount = 1.0;
    vec3 color = fragColor.rgb;
    if (material.hasBaseColorTexture == 1) {
        vec4 baseColor = texture(baseColorTexture, fragTexCoord);
        color = baseColor.rgb;

        if(material.hasUnlitColorTexture == 1) {
            vec4 unlitColor = texture(unlitColorTexture, fragTexCoord);

            mixAmount = 1. / (1. + exp(-20. * normalDot));
            mixAmount *= 1.0 + 5.0 * (diffuseDot - normalDot);
            mixAmount = clamp(mixAmount, 0.0, 1.0);
            color = mix(unlitColor, baseColor, mixAmount).rgb;
        }
    }

    
    if(material.hasSpecularTexture == 1) {
        float reflectRatio = 0.12;
        reflectRatio *= texture(specularTexture, fragTexCoord).r;
        reflectRatio += 0.03;

        vec3 viewDir = normalize(si.cameraPosition - worldPosition.xyz);
        vec3 viewDirTBN = world_to_TBN * viewDir;
        vec3 reflectDirTBN = reflect(-lightDirTBN, normalTBN);

        float specPower = clamp(max(dot(viewDirTBN, reflectDirTBN),0.), 0.0, 1.0);
        color += reflectRatio * pow(specPower, 2.0) * si.lightColor;
    }

    if (material.hasOverlayColorTexture == 1) {

        // Cloud shadow effect
        //vec3 originalNormal = vec3(0.0, 0.0, 1.0);
        vec3 shadowVec = 0.007 * TBN_to_world * (lightDirTBN - normalTBN) * normalDot;
        vec4 cloudShadow = texture(overlayColorTexture, fragTexCoord - shadowVec.xy);
        color *= (1.0 - cloudShadow.a * 0.5);

        // Cloud color effect
        float mixAmountHemisphere = clamp(1. / (1. + exp(-20. * normalDot)),0.0, 1.0);
        vec4 cloudsColor = texture(overlayColorTexture, fragTexCoord);
        cloudsColor.r *= clamp(mixAmountHemisphere, 0.1, 1.);
        cloudsColor.g *= clamp(pow(mixAmountHemisphere, 1.5), 0.1, 1.);
        cloudsColor.b *= clamp(pow(mixAmountHemisphere, 2.0), 0.1, 1.); // Blue light is less scattered than red light
        cloudsColor.a *= clamp(mixAmountHemisphere, 0.02, 1.);

        color = color * (1. - cloudsColor.a) + cloudsColor.rgb * cloudsColor.a;
    }

    if (material.sunShadeMode == 1) {
        // Distort UV coordinates for turbulence
        vec2 uv = fragTexCoord;
        uv = distortUV(uv, si.time * 0.3);

        // Scroll another noise layer for extra movement
        float scrollNoise = texture(overlayColorTexture, uv * 5.0 + vec2(si.time * 0.05, 0.0)).r;
        float turbulence = scrollNoise * 0.5;

        // Final color from sun texture with turbulence applied
        vec3 baseColor = texture(baseColorTexture, uv + turbulence * 0.01).rgb;

        // Boost color to simulate brightness
        color = baseColor; //* vec3(1.5, 1.2, 0.9); // warm tone
    }

    outColor = vec4(color, 1.0);
}

