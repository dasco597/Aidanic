#pragma once

#include <glm.hpp>

namespace Model {
    struct Sphere {
        glm::vec4 posRadius = glm::vec4(0.f);
        glm::vec4 color = glm::vec4(0.f);
    
        Sphere() {}
        Sphere(glm::vec3 position, float radius, glm::vec4 color) : posRadius(glm::vec4(position, radius)), color(color) {}
    };

    struct Ellipsoid {
        glm::vec4 center = glm::vec4(0.f);
        glm::vec4 radius = glm::vec4(0.f);
        glm::vec4 color = glm::vec4(0.f);
    
        Ellipsoid() {}
        Ellipsoid(glm::vec3 center, glm::vec3 radius, glm::vec4 color) :
            center(glm::vec4(center, 0.0)), radius(glm::vec4(radius, 0.0)), color(color) {}
    };
}
