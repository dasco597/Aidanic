#include "IOInterface.h"
#include "Aidanic.h"
#include "tools/Log.h"

void IOInterface::init(Aidanic* application, std::vector<const char*>& requiredExtensions, uint32_t width, uint32_t height) {
    AID_INFO("Initializing interface...");
    windowSize[0] = width;
    windowSize[1] = height;
    aidanicApp = application;
    if (window) cleanUp();

    // GLFW

    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        AID_ERROR("GLFW init failed");
    }

    // create glfw window
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // tells GLFW not to make an OpenGL context
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    window = glfwCreateWindow(width, height, "Aidanic", nullptr, nullptr);

    if (!glfwVulkanSupported()) {
        AID_ERROR("Vulkan not supported (GLFW check)");
    }

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, this->windowResizeCallback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwGetCursorPos(window, &mousePosPrev[0], &mousePosPrev[1]);

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    for (int e = 0; e < glfwExtensionCount; e++) {
        requiredExtensions.push_back(glfwExtensions[e]);
    }

    // DEAR IMGUI

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    imGuiIO = ImGui::GetIO();
    
    ImGui::StyleColorsDark();

    setKeyBindings();
}

void IOInterface::glfwErrorCallback(int error, const char* errorMessage) {
    AID_ERROR("GLFW Error {}: {}", error, errorMessage);
}

VkResult IOInterface::createVkSurface(VkInstance& instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface) {
    return glfwCreateWindowSurface(instance, window, allocator, surface);
}

void IOInterface::windowResizeCallback(GLFWwindow* window, int width, int height) {
    IOInterface* application = reinterpret_cast<IOInterface*>(glfwGetWindowUserPointer(window));
    application->aidanicApp->setWindowResizedFlag();
}

std::array<int, 2> IOInterface::getWindowSize() {
    std::array<int, 2> windowSize;
    glfwGetFramebufferSize(window, &windowSize[0], &windowSize[1]);
    return windowSize;
}

void IOInterface::minimizeSuspend() {
    int width = 0, height = 0;
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }
}

void IOInterface::pollEvents() {
    glfwPollEvents();
    mouseLeftClickDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1);

    // update control scheme
    if (glfwGetKey(window, keyBindingsInternal[INPUTS_INTERNAL::GAMEPLAY_MODE]) == GLFW_PRESS) {
        controlScheme = CONTROL_SCHEME::GAMEPLAY;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else if (glfwGetKey(window, keyBindingsInternal[INPUTS_INTERNAL::EDITOR_MODE]) == GLFW_PRESS){
        controlScheme = CONTROL_SCHEME::EDITOR;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
};

Inputs IOInterface::getInputs() {
    Inputs inputs = { INPUTS::NONE };
    for (auto& [key, input] : keyBindings) {
        int state = glfwGetKey(window, key);
        if (state == GLFW_PRESS) inputs.uint |= static_cast<uint32_t>(input);
    }
    return inputs;
}

std::array<double, 2> IOInterface::getMouseChange() {
    std::array<double, 2> deltaPos = { 0.0, 0.0 };
    double mousePosCurrent[2];
    glfwGetCursorPos(window, &mousePosCurrent[0], &mousePosCurrent[1]);

    switch (controlScheme) {
    case CONTROL_SCHEME::GAMEPLAY:
        deltaPos[0] = mousePosCurrent[0] - mousePosPrev[0];
        deltaPos[1] = mousePosCurrent[1] - mousePosPrev[1];
        break;
    case CONTROL_SCHEME::EDITOR:
        if (mouseLeftClickDown) {
            deltaPos[0] = mousePosCurrent[0] - mousePosPrev[0];
            deltaPos[1] = mousePosCurrent[1] - mousePosPrev[1];
        }
    }
    
    mousePosPrev[0] = mousePosCurrent[0];
    mousePosPrev[1] = mousePosCurrent[1];

    return deltaPos;
}

void IOInterface::cleanUp() {
    AID_INFO("Cleaning up ioInterface/GLFW...");
    glfwDestroyWindow(window);
    glfwTerminate();
    window = nullptr;
}

void IOInterface::setKeyBindings() {
    keyBindings[GLFW_KEY_ESCAPE] = INPUTS::ESC;

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

    keyBindingsInternal[INPUTS_INTERNAL::GAMEPLAY_MODE] = GLFW_KEY_1;
    keyBindingsInternal[INPUTS_INTERNAL::EDITOR_MODE]   = GLFW_KEY_2;
}