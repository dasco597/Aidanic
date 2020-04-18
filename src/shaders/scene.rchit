#version 460
#extension GL_NV_ray_tracing : require

#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

#define EPSILON 0.0001
#define AMBIENT 0.2

layout(set = 1, binding = 0) uniform accelerationStructureNV tlas;

hitAttributeNV HitPayload hit_payload;
layout(location = 0) rayPayloadInNV RayPayload ray_payload;
layout(location = 2) rayPayloadNV ShadowPayload shadow_payload; // todo location 1

void main()
{
    const vec3 light_source = vec3(-1.0, 5.0, 0.5);

    vec3 ray_o = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
    vec3 to_light = light_source - ray_o;
	const uint shadow_flags = gl_RayFlagsOpaqueNV;
	
    traceNV(tlas, shadow_flags, 0xFF, 1, 0, 1, ray_o + hit_payload.normal.xyz * EPSILON, 0.01, normalize(to_light), 1000.0, 2);
    
    float shadow;
    if (shadow_payload.in_shadow) {
        shadow = AMBIENT;
    } else {
        float diffuse = dot(normalize(to_light), normalize(hit_payload.normal.xyz));
        shadow = max(diffuse, AMBIENT);
    }

    vec4 color = hit_payload.color * shadow;
    ray_payload.color = color;
}
