#pragma once

#include "IOInterface.h"
#include "RendererRTX.h"

#include <GLFW/glfw3.h>
#include <glm.hpp>

class Aidanic {
public:
    ~Aidanic();
    void Run();

    static void WindowResizeCallback(GLFWwindow* window, int width, int height);

private:
    void Init();
    void Loop();
    void CleanUp();

    IOInterface IOinterface;
    RendererRTX rendererRTX;

    // variables
    bool quit = false, windowResized = false, cleanedUp = false;
    uint32_t windowSize[2] = { 0, 0 };
    uint32_t inputs = 0;
    glm::mat4x4 viewMatrix = glm::mat4x4(0.0);

    void ProcessInputs();
};