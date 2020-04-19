precision highp float;

// global config

#define ELLIPSOID_COUNT_PER_TLAS 8
#define MAX_MARCHING_STEPS 100
#define EPSILON 0.0001
#define MAX_DISTANCE 100.0

#define AMBIENT 0.2

// structs

struct HitPayload {
	vec4 color;
	vec4 normal;
};

struct RayPayload {
    vec4 color;
};

struct ShadowPayload {
    bool in_shadow;
	uint intersected_primitive_id;
};

struct Ellipsoid {
	vec4 center;
	vec4 radius;
	vec4 color;
};