#version 450

layout(push_constant) uniform PushConstants {
    mat4 model;
    uint objectId;
} pc;

layout(location = 0) out uint outObjectId;

void main() {
    outObjectId = pc.objectId;
}
