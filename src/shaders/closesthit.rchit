#version 460
#extension GL_NV_ray_tracing : require

layout(location = 0) rayPayloadInNV vec4 hitValue;

struct Sphere {
    vec4 posRadius;
};
layout(std430, binding = 0) readonly buffer Spheres { Sphere spheres[]; };

hitAttributeNV vec3 intersection;

void main()
{
    hitValue = vec4(0.3, 0.7, 0.9, 1.0);
}
