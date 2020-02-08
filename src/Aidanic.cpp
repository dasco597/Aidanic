#include "Aidanic.h"
#include "tools/config.h"
#include "tools/Log.h"

#include <iostream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

int main() {
    Aidanic app;
    std::cout << "I'm Aidanic, nice to meeet you!" << std::endl;
    try {
        app.Run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        //system("pause");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void Aidanic::Run() {
    Init();
    Loop();
    CleanUp();
}

Aidanic::~Aidanic() { if (!cleanedUp) CleanUp(); }

void Aidanic::Init() {
    Log::Init();
    LOG_INFO("Logger initialized");
    LOG_INFO("~ Initializing Aidanic...");

    windowSize[0] = _CONFIG::initialWindowSize[0];
    windowSize[1] = _CONFIG::initialWindowSize[1];

    ioInterface.Init(this, windowSize[0], windowSize[1]);
    LOG_INFO("IO interface initialized");
}

void Aidanic::Loop() {
    LOG_INFO("~ Entering main loop...");
    while (!quit && !ioInterface.WindowCloseCheck()) {
        // input handling
        ioInterface.PollEvents();
        inputs = ioInterface.GetInputs();
        ProcessInputs();
    }
}

void Aidanic::ProcessInputs() {
    quit |= (bool)(inputs & static_cast<uint32_t>(INPUTS::ESC));
}

void Aidanic::CleanUp() {
    LOG_INFO("~ Shutting down Aidanic...");

    ioInterface.CleanUp();
    LOG_INFO("IO interface cleaned up.");

    cleanedUp = true;
}

void Aidanic::WindowResizeCallback(GLFWwindow* window, int width, int height) {
    Aidanic* application = reinterpret_cast<Aidanic*>(glfwGetWindowUserPointer(window));
    application->windowResized = true;
}