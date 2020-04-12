#pragma once

namespace Model {
    struct Sphere {
        float pos[3] = { 0.f, 0.f, 0.f };
        float radius = 0.f;

        Sphere() {}
        Sphere(float x, float y, float z, float r) : pos{ x, y, z }, radius(r) {}
    };
}
