#include "Aidanic.h"
#include "tools/Log.h"

#include <gtx/rotate_vector.hpp>
#include <iostream>
#include <chrono>

using namespace std::chrono;

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

    updateMatrices();

    renderer.init(this, model, viewInverse, projInverse, viewerPosition);
    AID_INFO("Vulkan renderer RTX initialized");
}

VkResult Aidanic::createVkSurface(VkInstance& instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface) {
    return ioInterface.createVkSurface(instance, allocator, surface);
}

void Aidanic::loop() {
    AID_INFO("~ Entering main loop...");
    while (!quit && !ioInterface.windowCloseCheck()) {
        // submit draw commands for this frame
        renderer.drawFrame(windowResized, viewInverse, projInverse, viewerPosition);

        // input handling
        ioInterface.pollEvents();
        inputs = ioInterface.getInputs();
        processInputs();
    }
}

void Aidanic::processInputs() {
    quit |= inputs.conatinsInput(INPUTS::ESC);

    // get time difference
    static time_point<high_resolution_clock> timePrev = high_resolution_clock::now();
    double timeDif = duration<double, seconds::period>(high_resolution_clock::now() - timePrev).count();
    timePrev = high_resolution_clock::now();

    // get mouse inputs from window interface
    double mouseMovement[2];
    ioInterface.getMouseChange(mouseMovement[0], mouseMovement[1]);

    // LOOK

    float viewAngleHoriz = (double)mouseMovement[0] * -radiansPerMousePosYaw;
    viewerForward = glm::rotate(viewerForward, viewAngleHoriz, viewerUp);
    viewerLeft = glm::cross(viewerUp, viewerForward);

    float viewAngleVert = (double)mouseMovement[1] * -radiansPerMousePosPitch;
    viewerForward = glm::rotate(viewerForward, viewAngleVert, viewerLeft);
    viewerUp = glm::cross(viewerForward, viewerLeft);

    if (inputs.conatinsInput(INPUTS::ROTATEL) != inputs.conatinsInput(INPUTS::ROTATER)) {
        float viewAngleFront = -radiansPerSecondRoll * timeDif * (inputs.conatinsInput(INPUTS::ROTATER) ? 1 : -1);
        viewerUp = glm::rotate(viewerUp, viewAngleFront, viewerForward);
        viewerLeft = glm::cross(viewerUp, viewerForward);
    }

    viewerForward = glm::normalize(viewerForward);
    viewerLeft = glm::normalize(viewerLeft);
    viewerUp = glm::normalize(viewerUp);
    
    updateMatrices();

    // POSITION

    if (inputs.conatinsInput(INPUTS::FORWARD) != inputs.conatinsInput(INPUTS::BACKWARD))
        viewerPosition += viewerForward * glm::vec3(inputs.conatinsInput(INPUTS::FORWARD) ? forwardSpeed * timeDif : -backSpeed * timeDif);

    if (inputs.conatinsInput(INPUTS::LEFT) != inputs.conatinsInput(INPUTS::RIGHT))
        viewerPosition += viewerLeft * glm::vec3(inputs.conatinsInput(INPUTS::RIGHT) ? -strafeSpeed * timeDif : strafeSpeed * timeDif);

    if (inputs.conatinsInput(INPUTS::UP) != inputs.conatinsInput(INPUTS::DOWN))
        viewerPosition += viewerUp * glm::vec3(inputs.conatinsInput(INPUTS::UP) ? -strafeSpeed * timeDif : strafeSpeed * timeDif);
}

void Aidanic::updateMatrices() {
    projInverse = glm::inverse(glm::perspective(glm::radians(fovDegrees), static_cast<float>(windowSize[0] / windowSize[1]), nearPlane, farPlane));
    viewInverse = glm::inverse(glm::lookAt(viewerPosition, viewerPosition + viewerForward, viewerUp));
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