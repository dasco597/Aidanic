#pragma once

#include "IOInterface.h"
#include "RendererRTX.h"

#include <GLFW/glfw3.h>
#include <glm.hpp>

class Aidanic {
public:
    ~Aidanic();
    void Run();

    static void windowResizeCallback(GLFWwindow* window, int width, int height);

private:
    void init();
    void loop();
    void cleanUp();

    IOInterface IOInterface;
    RendererRTX rendererRTX;

    // variables
    bool _quit = false;
    uint32_t windowSize[2] = { 0, 0 };
    glm::mat4x4 viewMatrix = glm::mat4x4(0.0);
};