#include "Model.h"
#include "Renderer.h"
#include "tools/Log.h"

#include <map>
#include <stdexcept>
#include <algorithm>

using namespace Model;

namespace PrimitiveManager {

    // private variables

    std::vector<EllipsoidID> ellipsoidIDs;
    std::map<EllipsoidID, Ellipsoid> ellipsoids;

    // function implimentations

    Ellipsoid& getEllipsoidRef(EllipsoidID id) {
        if (!id.isValid()) {
            AID_WARN("ObjectManager::getEllipsoid() invalid id");
        }

        try {
            return ellipsoids.at(id);

        } catch (const std::out_of_range& e) {
            AID_ERROR("ObjectManager::getEllipsoid() ellipsoid not found with a valid id");
        }
    }

    EllipsoidID getNewEllipsoidID() {
        uint32_t idValue = 0;
        while (true) {
            EllipsoidID idTemp(idValue);
            if (std::find(ellipsoidIDs.begin(), ellipsoidIDs.end(), idTemp) == ellipsoidIDs.end()) // id not already taken
                return idTemp;
            idValue++;
        }
    }

    EllipsoidID addEllipsoid(Ellipsoid ellipsoid) {
        EllipsoidID id = getNewEllipsoidID();
        ellipsoids[id] = ellipsoid;
        ellipsoidIDs.push_back(id);

        Renderer::addEllipsoid(id);
        return id;
    }

    Ellipsoid getEllipsoid(EllipsoidID id) {
        return getEllipsoidRef(id);
    }

    void updateEllipsoid(EllipsoidID id, glm::vec3 center, glm::vec3 radius, glm::vec4 color) {
        Ellipsoid ellipsoid = getEllipsoidRef(id);
        ellipsoid.update(center, radius, color);
    }

    void deleteEllipsoid(EllipsoidID& id) {
        ellipsoids.erase(id);
        id.invalidate();
    }

    uint32_t getNumEllipsoids() { return ellipsoidIDs.size(); }

    std::vector<Model::EllipsoidID> getEllipsoidIDs() { return ellipsoidIDs; }
};