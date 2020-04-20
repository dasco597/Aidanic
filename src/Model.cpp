#include "Model.h"
#include "Renderer.h"
#include "tools/Log.h"

#include <map>
#include <stdexcept>

using namespace Model;

template <class ID_Class>
int Model::containsID(std::vector<ID_Class>& set, ID_Class id) {
    if (!id.isValid()) return -1;
    for (int i = 0; i < set.size(); i++) {
        if (set[i] == id) return i;
    }
    return -1;
}

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
            if (containsID(ellipsoidIDs, idTemp) == -1) // id not already taken
                return idTemp;
            idValue++;
        }
    }

    Ellipsoid getEllipsoid(EllipsoidID id) {
        return getEllipsoidRef(id);
    }

    EllipsoidID addEllipsoid(Ellipsoid ellipsoid) {
        EllipsoidID id = getNewEllipsoidID();
        ellipsoids[id] = ellipsoid;
        ellipsoidIDs.push_back(id);

        Renderer::addEllipsoid(id);
        return id;
    }

    void updateEllipsoid(EllipsoidID id, glm::vec3 center, glm::vec3 radius, glm::vec4 color) {
        Ellipsoid& ellipsoid = getEllipsoidRef(id);
        ellipsoid.update(center, radius, color);
        Renderer::updateEllipsoid(id);
    }

    void deleteEllipsoid(EllipsoidID& id) {
        Renderer::removeEllipsoid(id);
        ellipsoids.erase(id);
        ellipsoidIDs.erase(ellipsoidIDs.begin() + containsID(ellipsoidIDs, id));
        id.invalidate();
    }

    uint32_t getNumEllipsoids() { return ellipsoidIDs.size(); }

    std::vector<Model::EllipsoidID> getEllipsoidIDs() { return ellipsoidIDs; }
};