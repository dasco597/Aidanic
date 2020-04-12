#version 460
#extension GL_NV_ray_tracing : require

struct ShadowPayload {
    bool inShadow;
};
layout(location = 1) rayPayloadInNV ShadowPayload shadow_payload;

void main()
{
    shadow_payload.inShadow = false;
}