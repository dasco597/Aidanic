#version 460
#extension GL_NV_ray_tracing : require

struct RayPayload {
    vec4 color;
};
layout(location = 0) rayPayloadInNV RayPayload ray_payload;

void main()
{
    const float inv_factor = 0.1;
    const float darken_factor = 0.5;

    float inv_x = inv_factor / (abs(gl_WorldRayDirectionNV.x) + inv_factor);
    float inv_y = inv_factor / (abs(gl_WorldRayDirectionNV.y) + inv_factor);
    float inv_z = inv_factor / (abs(gl_WorldRayDirectionNV.z) + inv_factor);
    float color = darken_factor * max(max(inv_x, inv_y), inv_z);
    ray_payload.color = vec4(color, color, color, 1.0);
}