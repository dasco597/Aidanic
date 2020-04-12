#version 460
#extension GL_NV_ray_tracing : require

#define AMBIENT 0.2

struct Sphere {
    vec4 posRadius;
};
layout(set = 1, binding = 0) uniform accelerationStructureNV tlas;
layout(set = 1, binding = 1, std430) readonly buffer Spheres { Sphere spheres[]; };

struct HitPayload {
    vec4 intersection;
	vec4 color;
	vec4 normal;
};
hitAttributeNV HitPayload hit_payload;

struct RayPayload {
    vec4 color;
};
layout(location = 0) rayPayloadInNV RayPayload ray_payload;

//struct ShadowPayload {
//    bool in_shadow;
//}
//layout(location = 1) rayPayloadNV ShadowPayload shadow_payload;

void main()
{
    const vec3 light_source = vec3(-1.0, 0.5, 5.0);
    float diffuse = dot(normalize(light_source - hit_payload.intersection.xyz), normalize(hit_payload.normal.xyz));

    vec4 color = hit_payload.color * max(diffuse, AMBIENT);
    ray_payload.color = color;
}
