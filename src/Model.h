#pragma once

#include <glm.hpp>

namespace Model {
    struct Sphere {
        glm::vec4 posRadius = glm::vec4(0.f);
        glm::vec4 color = glm::vec4(0.f);

        Sphere() {}
        Sphere(glm::vec3 position, float radius, glm::vec4 color) : posRadius(glm::vec4(position, radius)), color(color) {}
    };
}
