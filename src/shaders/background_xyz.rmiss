#version 460
#extension GL_NV_ray_tracing : require

struct RayPayload {
    vec4 color;
};
layout(location = 0) rayPayloadInNV RayPayload ray_payload;

void main()
{
    ray_payload.color = vec4(0.5 * abs(gl_WorldRayDirectionNV) + 0.2, 1.0);
}