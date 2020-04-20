#include "Aidanic.h"

#include "Model.h"
#include "IOInterface.h"
#include "Renderer.h"
#include "ImGuiVk.h"
#include "tools/Log.h"
#include "tools/config.h"

#include "imgui.h"
#include "glm.hpp"
#include "gtx/rotate_vector.hpp"

#include <iostream>
#include <chrono>
#include <atomic>

using namespace std::chrono;

namespace Aidanic {

    // private function declarations

    void init();
    void initImGui();

    void loop();
    void updateImGui();
    void processInputs();
    void updateMatrices();

    void cleanup();

    // private variables

    bool quit = false;
    std::atomic<bool> windowResized = false;
    bool renderImGui = true;
    bool cleanedUp = false;

    Inputs inputs = { INPUTS::NONE };

    float fovDegrees = 90.f;
    float nearPlane = 0.1f;
    float farPlane = 10.f;

    float strafeSpeed = 4; // meters per second
    float forwardSpeed = 4;
    float backSpeed = 4;
    float radiansPerMousePosYaw = 0.001f; // horizontal - local y axis rotation
    float radiansPerMousePosPitch = 0.0008f; // vertical - local z axis rotation
    float radiansPerSecondRoll = _AID_PI / 4.0f; // local x axis rotation

    glm::vec3 viewerPosition = glm::vec3(-5.f, 0.f, 0.f);    // your position in the world
    glm::vec3 viewerForward = glm::vec3(1.f, 0.f, 0.f);	    // direction you are facing
    glm::vec3 viewerUp = glm::vec3(0.f, 1.f, 0.f);	    // viewer up direction
    glm::vec3 viewerRight = glm::vec3(0.f, 0.f, 1.f);     // cross product forward x up

    glm::mat4 viewInverse = glm::mat4(1.0f);
    glm::mat4 projInverse = glm::mat4(1.0f);

    void init() {
        Log::init();
        AID_INFO("Logger initialized");
        AID_INFO("~ Initializing Aidanic...");

        std::vector<const char*> requiredExtensions;
        IOInterface::init(requiredExtensions, _WINDOW_SIZE_X, _WINDOW_SIZE_Y);
        AID_INFO("IO interface initialized");

        updateMatrices();

        Renderer::init(requiredExtensions, viewInverse, projInverse, viewerPosition);
        AID_INFO("Vulkan renderer RTX initialized");

        initImGui();
        AID_INFO("ImGui initialized");
    }

    void initImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGuiVk::init();

        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        memcpy(ImGuiVk::getpClearValue(), &clear_color, 4 * sizeof(float));
    }

    void loop() {
        AID_INFO("~ Entering main loop...");
        while (!quit && !IOInterface::windowCloseCheck()) {
            // input handling
            IOInterface::pollEvents();
            inputs = IOInterface::getInputs();
            processInputs();

            // prepare ImGui
            if (renderImGui) updateImGui();

            // submit draw commands for this frame
            Renderer::drawFrame(windowResized, viewInverse, projInverse, viewerPosition, renderImGui);
        }
    }

    void updateImGui() {
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
        static Model::EllipsoidID selectedEllipsoid;

        // scene graph
        {
            ImGui::Begin("Scene");

            if (ImGui::Button("New ellipsoid")) {
                editorState = EditorState::NEW;
                selectedEllipsoid = Model::EllipsoidID();
            }

            std::vector<Model::EllipsoidID> ellipsoidIDs = PrimitiveManager::getEllipsoidIDs();
            for (Model::EllipsoidID id : ellipsoidIDs) {
                std::string message = std::string("Ellipsoid ") + std::to_string(id.getID());

                if (ImGui::Button(message.c_str())) {
                    editorState = EditorState::EDIT;
                    selectedEllipsoid = id;
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
                std::string message = std::string("Ellipsoid ") + std::to_string(selectedEllipsoid.getID());
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
            case EditorState::NEW:
                if (ImGui::Button("Add ellipsoid")) {
                    PrimitiveManager::addEllipsoid(Model::Ellipsoid(ellipsoidPos, ellipsoidRadius, ellipsoidColor));
                }
                break;

            case EditorState::EDIT:
                ImGui::Button("Update");
                ImGui::Button("Delete");
                break;
            }

            ImGui::End();
        }

        ImGui::Render();
    }

    void processInputs() {
        quit |= inputs.conatinsInput(INPUTS::ESC);

        // get time difference
        static time_point<high_resolution_clock> timePrev = high_resolution_clock::now();
        double timeDif = duration<double, seconds::period>(high_resolution_clock::now() - timePrev).count();
        timePrev = high_resolution_clock::now();

        // get mouse inputs from window interface
        double mouse_delta_x = 0, mouse_delta_y = 0;
        IOInterface::getMouseChange(mouse_delta_x, mouse_delta_y);

        // LOOK

        float viewAngleHoriz = (double)mouse_delta_x * -radiansPerMousePosPitch;
        viewerForward = glm::rotate(viewerForward, viewAngleHoriz, viewerUp);
        viewerRight = glm::cross(viewerForward, viewerUp);

        float viewAngleVert = (double)mouse_delta_y * -radiansPerMousePosYaw;
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

    void updateMatrices() {
        int width = 0, height = 0;
        IOInterface::getWindowSize(&width, &height);
        projInverse = glm::inverse(glm::perspective(glm::radians(fovDegrees), static_cast<float>(width / height), nearPlane, farPlane));
        viewInverse = glm::inverse(glm::lookAt(viewerPosition, viewerPosition + viewerForward, viewerUp));
    }

    void cleanup() {
        AID_INFO("~ Shutting down Aidanic...");

        ImGuiVk::cleanup();
        ImGui::DestroyContext();
        AID_INFO("ImGui cleaned up");

        Renderer::cleanUp();
        AID_INFO("Vulkan renderer RTX cleaned up");

        IOInterface::cleanUp();
        AID_INFO("IO interface cleaned up");

        cleanedUp = true;
    }

    void setWindowResizedFlag() { windowResized = true; }
};

// ENTRY POINT

int main() {
    std::cout << "I'm Aidanic, nice to meet you!" << std::endl;

    try {
        Aidanic::init();
        Aidanic::loop();
        Aidanic::cleanup();

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        system("pause");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
