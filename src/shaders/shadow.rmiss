#version 460
#extension GL_NV_ray_tracing : require

#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(location = 1) rayPayloadInNV ShadowPayload shadow_payload;

void main()
{
    shadow_payload.in_shadow = false;
}