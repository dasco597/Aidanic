#include "Model.h"
#include "Renderer.h"
#include "tools/Log.h"

#include <stdexcept>

using namespace Model;
/*
EllipsoidID PrimitiveManager::addEllipsoid(Ellipsoid ellipsoid) {
    EllipsoidID id(static_cast<int32_t>(numEllipsoids));
    ellipsoids.insert(std::make_pair(id, ellipsoid));
    numEllipsoids++;
    return id;
}

Ellipsoid PrimitiveManager::getEllipsoid(EllipsoidID id) {
    return getEllipsoidRef(id);
}

Ellipsoid& PrimitiveManager::getEllipsoidRef(EllipsoidID id) {
    if (!id.isValid()) {
        AID_WARN("ObjectManager::getEllipsoid() invalid id");
    }

    try {
        return ellipsoids.at(id);

    } catch (const std::out_of_range& e) {
        AID_ERROR("ObjectManager::getEllipsoid() ellipsoid not found with a valid id");
    }
}

void PrimitiveManager::updateEllipsoid(EllipsoidID id, glm::vec3 center, glm::vec3 radius, glm::vec4 color) {
    Ellipsoid& ellipsoid = getEllipsoidRef(id);
    ellipsoid.update(center, radius, color);
}

void PrimitiveManager::deleteEllipsoid(EllipsoidID& id) {
    ellipsoids.erase(id);
    id.invalidate();
}*/