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

Inputs IOInterface::getInputs() {
    Inputs inputs = { INPUTS::NONE };
    for (auto& [key, input] : keyBindings) {
        int state = glfwGetKey(window, key);
        if (state == GLFW_PRESS) inputs.uint |= input.uint;
    }
    return inputs;
}

void IOInterface::getMouseChange(double& mouseX, double& mouseY) {
    double mousePosCurrent[2];
    glfwGetCursorPos(window, &mousePosCurrent[0], &mousePosCurrent[1]); // get current mouse position
    mouseX = mousePosCurrent[0] - mousePosPrev[0];
    mouseY = mousePosCurrent[1] - mousePosPrev[1];
    glfwGetCursorPos(window, &mousePosPrev[0], &mousePosPrev[1]); // set previous position
}

void IOInterface::cleanUp() {
    AID_INFO("Cleaning up ioInterface/GLFW...");
    glfwDestroyWindow(window);
    glfwTerminate();
    window = nullptr;
}

void IOInterface::setKeyBindings() {
    keyBindings[GLFW_KEY_ESCAPE].i      = INPUTS::ESC;

    keyBindings[GLFW_KEY_W].i           = INPUTS::UP;
    keyBindings[GLFW_KEY_A].i           = INPUTS::LEFT;
    keyBindings[GLFW_KEY_S].i           = INPUTS::DOWN;
    keyBindings[GLFW_KEY_D].i           = INPUTS::RIGHT;
    keyBindings[GLFW_KEY_Q].i           = INPUTS::ROTATEL;
    keyBindings[GLFW_KEY_E].i           = INPUTS::ROTATER;

    keyBindings[GLFW_KEY_UP].i          = INPUTS::UP;
    keyBindings[GLFW_KEY_DOWN].i        = INPUTS::DOWN;
    keyBindings[GLFW_KEY_LEFT].i        = INPUTS::LEFT;
    keyBindings[GLFW_KEY_RIGHT].i       = INPUTS::RIGHT;
    keyBindings[GLFW_KEY_PAGE_UP].i     = INPUTS::ROTATEL;
    keyBindings[GLFW_KEY_PAGE_DOWN].i   = INPUTS::ROTATER;

    keyBindings[GLFW_KEY_SPACE].i       = INPUTS::FORWARD;
    keyBindings[GLFW_KEY_LEFT_SHIFT].i  = INPUTS::BACKWARD;

    keyBindings[GLFW_KEY_Z].i           = INPUTS::INTERACTL;
    keyBindings[GLFW_KEY_X].i           = INPUTS::INTERACTR;
}