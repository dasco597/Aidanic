#version 460
#extension GL_NV_ray_tracing : require

struct ShadowPayload {
    bool in_shadow;
};
layout(location = 2) rayPayloadInNV ShadowPayload shadow_payload;

void main()
{
    shadow_payload.in_shadow = true;
}