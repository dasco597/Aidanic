#version 460
#extension GL_NV_ray_tracing : require

#define AMBIENT 0.2

struct Sphere {
    vec4 posRadius;
};
layout(std430, binding = 0) readonly buffer Spheres { Sphere spheres[]; };

struct Hit_Payload {
    vec4 intersection;
	vec4 color;
	vec4 normal;
};
hitAttributeNV Hit_Payload hit_payload;

struct Ray_Payload {
    vec4 color;
};
layout(location = 0) rayPayloadInNV Ray_Payload ray_payload;

void main()
{
    const vec3 light_source = vec3(-1.0, 0.5, 5.0);
    float diffuse = dot(normalize(light_source - hit_payload.intersection.xyz), normalize(hit_payload.normal.xyz));

    vec4 color = hit_payload.color * max(diffuse, AMBIENT);
    ray_payload.color = color;
}
