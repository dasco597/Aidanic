#pragma once

#include "glm.hpp"
#include <vector>

namespace Model {

    class _ObjectID {
    public:
        _ObjectID() {}
        _ObjectID(int32_t id) : id(id) {}

        int32_t getID() { return id; }
        void invalidate() { id = -1; }
        bool isValid() { return id != -1; }

        bool operator == (const _ObjectID& other) const { return id == other.id; }
        bool operator <  (const _ObjectID& other) const { return id < other.id; }

    protected:
        int32_t id = -1;
    };

    class EllipsoidID : public _ObjectID {
        using _ObjectID::_ObjectID;
    };

    template <class ID_Class>
    int containsID(std::vector<ID_Class>& set, ID_Class id);

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
        int32_t objectID = -1;
        int32_t padding1 = 0, padding2 = 0, padding3 = 0;
    
        Ellipsoid() {}
        Ellipsoid(glm::vec3 center, glm::vec3 radius, glm::vec4 color, EllipsoidID id) :
            center(glm::vec4(center, 1.0)), radius(glm::vec4(radius, 1.0)), color(color), objectID(id.getID()) {}

        void update(glm::vec3 center, glm::vec3 radius, glm::vec4 color) {
            this->center = glm::vec4(center, 1.0);
            this->radius = glm::vec4(radius, 1.0);
            this->color = color;
        }
    };
}

namespace PrimitiveManager {
    Model::EllipsoidID addEllipsoid(glm::vec3 center, glm::vec3 radius, glm::vec4 color);
    void updateEllipsoid(Model::EllipsoidID id, glm::vec3 center, glm::vec3 radius, glm::vec4 color);
    void deleteEllipsoid(Model::EllipsoidID& id);

    Model::Ellipsoid getEllipsoid(Model::EllipsoidID id);
    uint32_t getNumEllipsoids();
    std::vector<Model::EllipsoidID> getEllipsoidIDs();
};