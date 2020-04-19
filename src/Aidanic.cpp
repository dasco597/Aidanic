#include "Aidanic.h"
#include "Model.h"
#include "tools/Log.h"

#include <gtx/rotate_vector.hpp>
#include <iostream>
#include <chrono>

using namespace std::chrono;

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
    IOInterface::init(this, requiredExtensions, windowSize[0], windowSize[1]);
    AID_INFO("IO interface initialized");

    updateMatrices();

    renderer.init(this, requiredExtensions, viewInverse, projInverse, viewerPosition);
    AID_INFO("Vulkan renderer RTX initialized");

    initImGui();
    AID_INFO("ImGui initialized");
}

void Aidanic::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    imGuiRenderer.init(&renderer);

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    memcpy(imGuiRenderer.getpClearValue(), &clear_color, 4 * sizeof(float));
}

VkResult Aidanic::createVkSurface(VkInstance& instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface) {
    return IOInterface::createVkSurface(instance, allocator, surface);
}

void Aidanic::loop() {
    AID_INFO("~ Entering main loop...");
    while (!quit && !IOInterface::windowCloseCheck()) {
        // input handling
        IOInterface::pollEvents();
        inputs = IOInterface::getInputs();
        processInputs();

        // prepare ImGui
        if (renderImGui) updateImGui();

        // submit draw commands for this frame
        renderer.drawFrame(windowResized, viewInverse, projInverse, viewerPosition, &imGuiRenderer, renderImGui);
    }
}

void Aidanic::updateImGui() {
    ImGuiIO& io = ImGui::GetIO();
    if (!io.Fonts->IsBuilt()) {
        AID_WARN("imGui font atlas not built!");
    }

    IOInterface::updateImGui();
    ImGui::NewFrame();

    enum struct EditorState {
        NEW,
        EDIT
    };

    static EditorState editorState = EditorState::NEW;
    static int selectedEllipsoid = -1;

    // scene graph
    {
        ImGui::Begin("Scene");

        if (ImGui::Button("New ellipsoid")) {
            editorState = EditorState::NEW;
            selectedEllipsoid = -1;
        }
        for (int e = 0; e < ellipsoids.size(); e++) {
            std::string message = std::string("Ellipsoid ") + std::to_string(e);
            if (ImGui::Button(message.c_str())) {
                editorState = EditorState::EDIT;
                selectedEllipsoid = e;
            }
        }

        ImGui::End();
    }

    // object editor
    {
        ImGui::Begin("Editor");
        switch (editorState)
        {
        case EditorState::NEW:
            ImGui::Text("New ellipsoid"); break;

        case EditorState::EDIT:
            std::string message = std::string("Ellipsoid ") + std::to_string(selectedEllipsoid);
            ImGui::Text(message.c_str()); break;
        }

        // ellipsoid parameters
        static glm::vec3 ellipsoidPos = glm::vec3(0.f);
        static glm::vec3 ellipsoidRadius = glm::vec3(0.5f);
        static glm::vec4 ellipsoidColor = glm::vec4(1.0f);

        ImGui::SliderFloat("pos x", &ellipsoidPos.x, -2.0f, 2.0f);
        ImGui::SliderFloat("pos y", &ellipsoidPos.y, -2.0f, 2.0f);
        ImGui::SliderFloat("pos z", &ellipsoidPos.z, -2.0f, 2.0f);
        ImGui::SliderFloat("radius x", &ellipsoidRadius.x, 0.0f, 2.0f);
        ImGui::SliderFloat("radius y", &ellipsoidRadius.y, 0.0f, 2.0f);
        ImGui::SliderFloat("radius z", &ellipsoidRadius.z, 0.0f, 2.0f);
        ImGui::ColorEdit3("color", &ellipsoidColor.r);

        switch (editorState)
        {
        case EditorState::NEW :
            if (ImGui::Button("Add ellipsoid")) {
                ellipsoids.push_back(Model::Ellipsoid(ellipsoidPos, ellipsoidRadius, ellipsoidColor));
                renderer.addEllipsoid(ellipsoids[ellipsoids.size() - 1]);
            }
            break;

        case EditorState::EDIT :
            ImGui::Button("Update");
            ImGui::Button("Delete");
            break;
        }

        ImGui::End();
    }

    ImGui::Render();
}

void Aidanic::processInputs() {
    quit |= inputs.conatinsInput(INPUTS::ESC);

    // get time difference
    static time_point<high_resolution_clock> timePrev = high_resolution_clock::now();
    double timeDif = duration<double, seconds::period>(high_resolution_clock::now() - timePrev).count();
    timePrev = high_resolution_clock::now();

    // get mouse inputs from window interface
    std::array<double, 2> mouseMovement = IOInterface::getMouseChange();

    // LOOK

    float viewAngleHoriz = (double)mouseMovement[0] * -radiansPerMousePosPitch;
    viewerForward = glm::rotate(viewerForward, viewAngleHoriz, viewerUp);
    viewerRight = glm::cross(viewerForward, viewerUp);

    float viewAngleVert = (double)mouseMovement[1] * -radiansPerMousePosYaw;
    viewerForward = glm::rotate(viewerForward, viewAngleVert, viewerRight);
    viewerUp = glm::cross(viewerRight, viewerForward);

    if (inputs.conatinsInput(INPUTS::ROTATEL) != inputs.conatinsInput(INPUTS::ROTATER)) {
        float viewAngleFront = radiansPerSecondRoll * timeDif * (inputs.conatinsInput(INPUTS::ROTATER) ? 1 : -1);
        viewerUp = glm::rotate(viewerUp, viewAngleFront, viewerForward);
        viewerRight = glm::cross(viewerForward, viewerUp);
    }

    viewerForward = glm::normalize(viewerForward);
    viewerRight = glm::normalize(viewerRight);
    viewerUp = glm::normalize(viewerUp);
    
    updateMatrices();

    // POSITION

    if (inputs.conatinsInput(INPUTS::FORWARD) != inputs.conatinsInput(INPUTS::BACKWARD))
        viewerPosition += viewerForward * glm::vec3(inputs.conatinsInput(INPUTS::FORWARD) ? forwardSpeed * timeDif : -backSpeed * timeDif);

    if (inputs.conatinsInput(INPUTS::LEFT) != inputs.conatinsInput(INPUTS::RIGHT))
        viewerPosition += viewerRight * glm::vec3(inputs.conatinsInput(INPUTS::RIGHT) ? strafeSpeed * timeDif : -strafeSpeed * timeDif);

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

    IOInterface::cleanUp();
    AID_INFO("IO interface cleaned up");

    cleanedUp = true;
}

std::array<int, 2> Aidanic::getWindowSize() {
    windowSize = IOInterface::getWindowSize();
    return windowSize;
}