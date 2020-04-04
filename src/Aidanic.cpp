#include "Aidanic.h"
#include "tools/Log.h"

#include <iostream>

std::vector<Vertex> vertices = {
    { {  1.0f,  1.0f, 0.0f } },
    { { -1.0f,  1.0f, 0.0f } },
    { {  0.0f, -1.0f, 0.0f } }
};
std::vector<uint32_t> indices = { 0, 1, 2 };

int main() {
    Aidanic app;
    std::cout << "I'm Aidanic, nice to meet you!" << std::endl;

    try {
        app.Run();

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        system("pause");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void Aidanic::Run() {
    init();
    loop();
    cleanUp();
}

void Aidanic::init() {
    Log::init();
    AID_INFO("Logger initialized");
    AID_INFO("~ Initializing Aidanic...");

    ioInterface.init(this, windowSize[0], windowSize[1]);
    AID_INFO("IO interface initialized");

    Model model{
        std::vector<Vertex>(vertices),
        std::vector<uint32_t>(indices)
    };

    renderer.init(this, model);
    AID_INFO("Vulkan renderer RTX initialized");
}

VkResult Aidanic::createVkSurface(VkInstance& instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface) {
    return ioInterface.createVkSurface(instance, allocator, surface);
}

void Aidanic::loop() {
    AID_INFO("~ Entering main loop...");
    while (!quit && !ioInterface.windowCloseCheck()) {
        // submit draw commands for this frame
        renderer.drawFrame(windowResized);

        // input handling
        ioInterface.pollEvents();
        inputs = ioInterface.getInputs();
        processInputs();
    }
}

void Aidanic::processInputs() {
    quit |= (bool)(inputs.uint & IO_INPUT_UINT(ESC));
}

void Aidanic::cleanUp() {
    AID_INFO("~ Shutting down Aidanic...");

    renderer.cleanUp();
    AID_INFO("Vulkan renderer RTX cleaned up");

    ioInterface.cleanUp();
    AID_INFO("IO interface cleaned up");

    cleanedUp = true;
}

void Aidanic::windowResizeCallback(GLFWwindow* window, int width, int height) {
    Aidanic* application = reinterpret_cast<Aidanic*>(glfwGetWindowUserPointer(window));
    application->windowResized = true;
}

std::array<int, 2> Aidanic::getWindowSize() {
    glfwGetFramebufferSize(ioInterface.getWindow(), &windowSize[0], &windowSize[1]);
    return windowSize;
}