#pragma once

#include "IOInterface.h"
#include "Renderer.h"
#include "ImGuiVk.h"
#include "tools/config.h"

#include <imgui.h>
#include <glm.hpp>
#include <atomic>

class Aidanic {
public:
    void Run();

    VkResult createVkSurface(VkInstance& instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface);
    std::array<int, 2> getWindowSize();
    void setWindowResizedFlag() { windowResized = true; }

private:
    void init();
    void initImGui();

    void loop();
    void updateImGui();
    void processInputs();
    void updateMatrices();

    void cleanup();

    // variables

    bool quit = false;
    std::atomic<bool> windowResized = false;
    bool renderImGui = true;
    bool cleanedUp = false;

    std::array<int, 2> windowSize = { _WINDOW_SIZE_X, _WINDOW_SIZE_Y };
    Inputs inputs = { INPUTS::NONE };

    std::vector<Model::Ellipsoid> ellipsoids;

    float fovDegrees = 90.f;
    float nearPlane = 0.1f;
    float farPlane = 10.f;

    float strafeSpeed = 4; // meters per second
    float forwardSpeed = 4;
    float backSpeed = 4;
    float radiansPerMousePosYaw = 0.001f; // horizontal - local y axis rotation
    float radiansPerMousePosPitch = 0.0008f; // vertical - local z axis rotation
    float radiansPerSecondRoll = _AID_PI / 4.0f; // local x axis rotation

    glm::vec3 viewerPosition    = glm::vec3(-5.f, 0.f, 0.f);    // your position in the world
    glm::vec3 viewerForward     = glm::vec3(1.f, 0.f, 0.f);	    // direction you are facing
    glm::vec3 viewerUp          = glm::vec3(0.f, 1.f, 0.f);	    // viewer up direction
    glm::vec3 viewerRight       = glm::vec3(0.f, 0.f, 1.f);     // cross product forward x up

    glm::mat4 viewInverse = glm::mat4(1.0f);
    glm::mat4 projInverse = glm::mat4(1.0f);
};