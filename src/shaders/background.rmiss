#version 460
#extension GL_NV_ray_tracing : require

#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(location = 0) rayPayloadInNV RayPayload ray_payload;

void main()
{
    vec3 ro = gl_WorldRayOriginNV;
    vec3 rd = gl_WorldRayDirectionNV;

    const vec3 sky = vec3(0.3, 0.4, 0.5) + 0.3 * rd.y;

    ray_payload.color = vec4(sky, 1.0);
}