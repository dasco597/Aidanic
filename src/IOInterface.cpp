#include "IOInterface.h"
#include "Aidanic.h"
#include "tools/Log.h"

void IOInterface::Init(Aidanic* application, uint32_t width, uint32_t height) {
    LOG_INFO("Initializing interface...");
    windowSize[0] = width;
    windowSize[1] = height;
    if (window) CleanUp();

    // glfw is the window/input manager we'll be using
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // tells GLFW not to make an OpenGL context
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

    // create glfw window
    window = glfwCreateWindow(width, height, "Aidanic", nullptr, nullptr);
    glfwSetWindowUserPointer(window, application);
    glfwSetFramebufferSizeCallback(window, application->WindowResizeCallback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwGetCursorPos(window, &mousePosPrev[0], &mousePosPrev[1]);

    SetKeyBindings();
}

void IOInterface::MinimizeSuspend() {
    int width = 0, height = 0;
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }
}

uint32_t IOInterface::GetInputs() {
    uint32_t inputs = 0;
    for (auto& [key, input] : keyBindings) {
        int state = glfwGetKey(window, key);
        if (state == GLFW_PRESS) inputs |= static_cast<uint32_t>(input);
    }
    return inputs;
}

void IOInterface::CleanUp() {
    LOG_INFO("Cleaning up ioInterface/GLFW...");
    glfwDestroyWindow(window);
    glfwTerminate();
    window = nullptr;
}

void IOInterface::SetKeyBindings() {
    keyBindings[GLFW_KEY_ESCAPE]        = INPUTS::ESC;

    keyBindings[GLFW_KEY_W]             = INPUTS::UP;
    keyBindings[GLFW_KEY_A]             = INPUTS::LEFT;
    keyBindings[GLFW_KEY_S]             = INPUTS::DOWN;
    keyBindings[GLFW_KEY_D]             = INPUTS::RIGHT;
    keyBindings[GLFW_KEY_Q]             = INPUTS::ROTATEL;
    keyBindings[GLFW_KEY_E]             = INPUTS::ROTATER;

    keyBindings[GLFW_KEY_UP]            = INPUTS::UP;
    keyBindings[GLFW_KEY_DOWN]          = INPUTS::DOWN;
    keyBindings[GLFW_KEY_LEFT]          = INPUTS::LEFT;
    keyBindings[GLFW_KEY_RIGHT]         = INPUTS::RIGHT;
    keyBindings[GLFW_KEY_PAGE_UP]       = INPUTS::ROTATEL;
    keyBindings[GLFW_KEY_PAGE_DOWN]     = INPUTS::ROTATER;

    keyBindings[GLFW_KEY_SPACE]         = INPUTS::FORWARD;
    keyBindings[GLFW_KEY_LEFT_SHIFT]    = INPUTS::BACKWARD;

    keyBindings[GLFW_KEY_Z]             = INPUTS::INTERACTL;
    keyBindings[GLFW_KEY_X]             = INPUTS::INTERACTR;
}