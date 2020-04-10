#include "Aidanic.h"
#include "tools/Log.h"

#include <gtx/rotate_vector.hpp>
#include <iostream>
#include <chrono>

using namespace std::chrono;

std::vector<Vertex> vertices = {
    { { 5.0f,  0.0f,  0.577f } },
    { { 5.0f, -0.5f, -0.289f } },
    { { 5.0f,  0.5f, -0.289f } }
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
    cleanup();
}

void Aidanic::init() {
    Log::init();
    AID_INFO("Logger initialized");
    AID_INFO("~ Initializing Aidanic...");

    std::vector<const char*> requiredExtensions;
    ioInterface.init(this, requiredExtensions, windowSize[0], windowSize[1]);
    AID_INFO("IO interface initialized");

    Model model{ 
        std::vector<Vertex>(vertices),
        std::vector<uint32_t>(indices)
    };

    updateMatrices();

    renderer.init(this, requiredExtensions, model, viewInverse, projInverse, viewerPosition);
    AID_INFO("Vulkan renderer RTX initialized");

    initImGui();
    AID_INFO("ImGui initialized");
}

void Aidanic::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    imGuiRenderer.init(&renderer);
}

VkResult Aidanic::createVkSurface(VkInstance& instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface) {
    return ioInterface.createVkSurface(instance, allocator, surface);
}

void Aidanic::loop() {
    AID_INFO("~ Entering main loop...");
    while (!quit && !ioInterface.windowCloseCheck()) {
        // input handling
        ioInterface.pollEvents();
        inputs = ioInterface.getInputs();
        processInputs();

        // prepare ImGui
        if (renderImGui) updateImGui();

        // submit draw commands for this frame
        renderer.drawFrame(windowResized, viewInverse, projInverse, viewerPosition, renderImGui, &imGuiRenderer);
    }
}

void Aidanic::updateImGui() {
    ImGuiIO& io = ImGui::GetIO();
    if (!io.Fonts->IsBuilt()) {
        AID_WARN("imGui font atlas not built!");
    }

    ioInterface.updateImGui();
    ImGui::NewFrame();

    // test window
    static bool show_window = true;
    static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &show_window);      // Edit bools storing our window open/close state

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();
    }

    ImGui::Render();
    memcpy(imGuiRenderer.getpClearValue(), &clear_color, 4 * sizeof(float));
}

void Aidanic::processInputs() {
    quit |= inputs.conatinsInput(INPUTS::ESC);

    // get time difference
    static time_point<high_resolution_clock> timePrev = high_resolution_clock::now();
    double timeDif = duration<double, seconds::period>(high_resolution_clock::now() - timePrev).count();
    timePrev = high_resolution_clock::now();

    // get mouse inputs from window interface
    std::array<double, 2> mouseMovement = ioInterface.getMouseChange();

    // LOOK

    float viewAngleHoriz = (double)mouseMovement[0] * -radiansPerMousePosYaw;
    viewerForward = glm::rotate(viewerForward, viewAngleHoriz, viewerUp);
    viewerLeft = glm::cross(viewerUp, viewerForward);

    float viewAngleVert = (double)mouseMovement[1] * radiansPerMousePosPitch;
    viewerForward = glm::rotate(viewerForward, viewAngleVert, viewerLeft);
    viewerUp = glm::cross(viewerForward, viewerLeft);

    if (inputs.conatinsInput(INPUTS::ROTATEL) != inputs.conatinsInput(INPUTS::ROTATER)) {
        float viewAngleFront = radiansPerSecondRoll * timeDif * (inputs.conatinsInput(INPUTS::ROTATER) ? 1 : -1);
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
        viewerPosition += viewerUp * glm::vec3(inputs.conatinsInput(INPUTS::UP) ? strafeSpeed * timeDif : -strafeSpeed * timeDif);
}

void Aidanic::updateMatrices() {
    projInverse = glm::inverse(glm::perspective(glm::radians(fovDegrees), static_cast<float>(windowSize[0] / windowSize[1]), nearPlane, farPlane));
    viewInverse = glm::inverse(glm::lookAt(viewerPosition, viewerPosition + viewerForward, viewerUp));
}

void Aidanic::cleanup() {
    AID_INFO("~ Shutting down Aidanic...");

    imGuiRenderer.cleanup();
    ImGui::DestroyContext();
    AID_INFO("ImGui cleaned up");

    renderer.cleanUp();
    AID_INFO("Vulkan renderer RTX cleaned up");

    ioInterface.cleanUp();
    AID_INFO("IO interface cleaned up");

    cleanedUp = true;
}

std::array<int, 2> Aidanic::getWindowSize() {
    windowSize = ioInterface.getWindowSize();
    return windowSize;
}