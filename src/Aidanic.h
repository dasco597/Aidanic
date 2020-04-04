#pragma once

#include "IOInterface.h"
#include "Renderer.h"
#include "tools/config.h"

#include <glm.hpp>

class Aidanic {
public:
    void Run();

    VkResult createVkSurface(VkInstance& instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface);
    static void windowResizeCallback(GLFWwindow* window, int width, int height);

    std::array<int, 2> getWindowSize();

private:
    void init();

    void loop();
    void processInputs();

    void cleanUp();

    IOInterface ioInterface;
    Renderer renderer;

    // variables

    bool quit = false;
    bool windowResized = false;
    bool cleanedUp = false;

    std::array<int, 2> windowSize = { _WINDOW_SIZE_X, _WINDOW_SIZE_Y };
    IOInterface::Inputs inputs = { IOInterface::INPUTS::NONE };
    glm::mat4x4 viewMatrix = glm::mat4x4(0.0);
};