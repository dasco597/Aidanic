#include "IOInterface.h"
#include "Aidanic.h"
#include "tools/Log.h"
#include <imgui.h>

namespace IOInterface {

    // structs

    enum struct CONTROL_SCHEME {
        EDITOR, // free mouse controls (activate with key_1)
        GAMEPLAY // captured mouse (activate with key_2)
    };

    struct WindowApp {
        Aidanic* aidanicApp;
        static void windowResizeCallback(GLFWwindow* window, int width, int height);
    };

    // private variables

    GLFWwindow* window = nullptr;
    WindowApp windowApp;

    double time = 0.0, timeDelta = 0.0;
    bool mouseLeftClickDown = false;
    uint32_t windowSize[2] = { 0, 0 };
    double mousePosPrev[2] = { 0.0, 0.0 };
    CONTROL_SCHEME controlScheme = CONTROL_SCHEME::EDITOR;

    enum struct INPUTS_INTERNAL {
        EDITOR_MODE,
        GAMEPLAY_MODE
    };
    std::map<uint32_t, INPUTS> keyBindings;
    std::map<INPUTS_INTERNAL, uint32_t> keyBindingsInternal;

    // private functions

    void setKeyBindings();

    // public function implimentations

    void init(Aidanic* application, std::vector<const char*>& requiredExtensions, uint32_t width, uint32_t height) {
        AID_INFO("Initializing interface...");
        windowSize[0] = width;
        windowSize[1] = height;
        windowApp.aidanicApp = application;
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

        glfwSetWindowUserPointer(window, &windowApp);
        glfwSetFramebufferSizeCallback(window, windowApp.windowResizeCallback);

        switch (controlScheme) {
        case CONTROL_SCHEME::EDITOR: glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); break;
        case CONTROL_SCHEME::GAMEPLAY: glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); break;
        default: glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        glfwGetCursorPos(window, &mousePosPrev[0], &mousePosPrev[1]);

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        for (int e = 0; e < glfwExtensionCount; e++) {
            requiredExtensions.push_back(glfwExtensions[e]);
        }

        setKeyBindings();
    }

    void glfwErrorCallback(int error, const char* errorMessage) {
        AID_ERROR("GLFW Error {}: {}", error, errorMessage);
    }

    VkResult createVkSurface(VkInstance& instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface) {
        return glfwCreateWindowSurface(instance, window, allocator, surface);
    }

    void WindowApp::windowResizeCallback(GLFWwindow* window, int width, int height) {
        WindowApp* application = reinterpret_cast<WindowApp*>(glfwGetWindowUserPointer(window));
        application->aidanicApp->setWindowResizedFlag();
    }

    std::array<int, 2> getWindowSize() {
        std::array<int, 2> windowSize;
        glfwGetFramebufferSize(window, &windowSize[0], &windowSize[1]);
        return windowSize;
    }

    int windowCloseCheck() {
        return glfwWindowShouldClose(window);
    };

    void minimizeSuspend() {
        int width = 0, height = 0;
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }
    }

    void pollEvents() {
        ImGuiIO& io = ImGui::GetIO();

        glfwPollEvents();
        mouseLeftClickDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) && !io.MouseDownOwned[GLFW_MOUSE_BUTTON_1];

        // update control scheme
        if (glfwGetKey(window, keyBindingsInternal[INPUTS_INTERNAL::GAMEPLAY_MODE]) == GLFW_PRESS) {
            controlScheme = CONTROL_SCHEME::GAMEPLAY;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else if (glfwGetKey(window, keyBindingsInternal[INPUTS_INTERNAL::EDITOR_MODE]) == GLFW_PRESS) {
            controlScheme = CONTROL_SCHEME::EDITOR;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }

        double current_time = glfwGetTime();
        timeDelta = time > 0.0 ? (float)(current_time - time) : (float)(1.0f / 60.0f);
        time = current_time;
    }

    void updateImGui() {
        ImGuiIO& io = ImGui::GetIO();

        // Setup display size (every frame to accommodate for window resizing)
        int w, h;
        int display_w, display_h;
        glfwGetWindowSize(window, &w, &h);
        glfwGetFramebufferSize(window, &display_w, &display_h);
        io.DisplaySize = ImVec2((float)w, (float)h);
        if (w > 0 && h > 0)
            io.DisplayFramebufferScale = ImVec2((float)display_w / w, (float)display_h / h);

        // Setup time step
        io.DeltaTime = timeDelta;

        // mouse buttons
        static bool mouseJustPressed[5] = { false, false, false, false, false };
        for (int i = 0; i < 5; i++) {
            // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
            io.MouseDown[i] = mouseJustPressed[i] || glfwGetMouseButton(window, i) != 0;
            mouseJustPressed[i] = false;
        }

        // Update mouse position
        const ImVec2 mouse_pos_backup = io.MousePos;
        io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
        const bool focused = glfwGetWindowAttrib(window, GLFW_FOCUSED) != 0;
        if (focused) {
            if (io.WantSetMousePos) {
                glfwSetCursorPos(window, (double)mouse_pos_backup.x, (double)mouse_pos_backup.y);
            } else {
                double mouse_x, mouse_y;
                glfwGetCursorPos(window, &mouse_x, &mouse_y);
                io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);
            }
        }

        // mouse cursor
        if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) || glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
            return;

        static GLFWcursor* g_MouseCursors[ImGuiMouseCursor_COUNT] = {};
        ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
        if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor) {
            // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        } else {
            // Show OS mouse cursor
            // FIXME-PLATFORM: Unfocused windows seems to fail changing the mouse cursor with GLFW 3.2, but 3.3 works here.
            glfwSetCursor(window, g_MouseCursors[imgui_cursor] ? g_MouseCursors[imgui_cursor] : g_MouseCursors[ImGuiMouseCursor_Arrow]);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }

    Inputs getInputs() {
        Inputs inputs = { INPUTS::NONE };
        for (auto& [key, input] : keyBindings) {
            int state = glfwGetKey(window, key);
            if (state == GLFW_PRESS) inputs.uint |= static_cast<uint32_t>(input);
        }
        return inputs;
    }

    std::array<double, 2> getMouseChange() {
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
            break;
        }

        mousePosPrev[0] = mousePosCurrent[0];
        mousePosPrev[1] = mousePosCurrent[1];

        return deltaPos;
    }

    void cleanUp() {
        AID_INFO("Cleaning up ioInterface/GLFW...");
        glfwDestroyWindow(window);
        glfwTerminate();
        window = nullptr;
    }

    // private function implimentations

    void setKeyBindings() {
        keyBindings[GLFW_KEY_ESCAPE] = INPUTS::ESC;

        keyBindings[GLFW_KEY_W] = INPUTS::UP;
        keyBindings[GLFW_KEY_A] = INPUTS::LEFT;
        keyBindings[GLFW_KEY_S] = INPUTS::DOWN;
        keyBindings[GLFW_KEY_D] = INPUTS::RIGHT;
        keyBindings[GLFW_KEY_Q] = INPUTS::ROTATEL;
        keyBindings[GLFW_KEY_E] = INPUTS::ROTATER;

        keyBindings[GLFW_KEY_UP] = INPUTS::UP;
        keyBindings[GLFW_KEY_DOWN] = INPUTS::DOWN;
        keyBindings[GLFW_KEY_LEFT] = INPUTS::LEFT;
        keyBindings[GLFW_KEY_RIGHT] = INPUTS::RIGHT;
        keyBindings[GLFW_KEY_PAGE_UP] = INPUTS::ROTATEL;
        keyBindings[GLFW_KEY_PAGE_DOWN] = INPUTS::ROTATER;

        keyBindings[GLFW_KEY_SPACE] = INPUTS::FORWARD;
        keyBindings[GLFW_KEY_LEFT_SHIFT] = INPUTS::BACKWARD;

        keyBindings[GLFW_KEY_Z] = INPUTS::INTERACTL;
        keyBindings[GLFW_KEY_X] = INPUTS::INTERACTR;

        keyBindingsInternal[INPUTS_INTERNAL::EDITOR_MODE] = GLFW_KEY_1;
        keyBindingsInternal[INPUTS_INTERNAL::GAMEPLAY_MODE] = GLFW_KEY_2;
    }

};