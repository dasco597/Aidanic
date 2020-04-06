#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <map>
#include <vector>

enum struct CONTROL_SCHEME {
	GAMEPLAY, // captured mouse (activate with key_1)
	EDITOR // free mouse controls (activate with key_2)
};

class Aidanic;

enum struct INPUTS {
	NONE		= 0,
	ESC			= (1 << 0),
	FORWARD		= (1 << 1),
	BACKWARD	= (1 << 2),
	LEFT		= (1 << 3),
	RIGHT		= (1 << 4),
	UP			= (1 << 5),
	DOWN		= (1 << 6),
	ROTATEL		= (1 << 7),
	ROTATER		= (1 << 8),
	MOUSEL		= (1 << 9),
	MOUSER		= (1 << 10),
	INTERACTL	= (1 << 11),
	INTERACTR	= (1 << 12),
}; // do not excede 32 values!

union Inputs {
	INPUTS i;
	uint32_t uint; // used for bitwise operations (one input type per bit)
	bool conatinsInput(INPUTS compValue) { return uint & static_cast<uint32_t>(compValue); }
};

class IOInterface {
public:

    void init(Aidanic* application, std::vector<const char*>& requiredExtensions, uint32_t width, uint32_t height);
	static void glfwErrorCallback(int error, const char* errorMessage);
	VkResult createVkSurface(VkInstance& instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface);

	static void windowResizeCallback(GLFWwindow* window, int width, int height);
	std::array<int, 2> IOInterface::getWindowSize();

	int windowCloseCheck() { return glfwWindowShouldClose(window); };
	void minimizeSuspend(); // doesn't return unless window isn't minimized
	void pollEvents(); // updates glfw state (key presses etc)

	Inputs getInputs();
	std::array<double, 2> getMouseChange();

	void cleanUp();

private:

	Aidanic* aidanicApp;
    GLFWwindow* window = nullptr;
	ImGuiIO imGuiIO;

	bool mouseLeftClickDown = false;
    uint32_t windowSize[2] = { 0, 0 };
	double mousePosPrev[2] = { 0.0, 0.0 };
	CONTROL_SCHEME controlScheme = CONTROL_SCHEME::GAMEPLAY;

	enum struct INPUTS_INTERNAL {
		GAMEPLAY_MODE,
		EDITOR_MODE
	};
    std::map<uint32_t, INPUTS> keyBindings;
	std::map<INPUTS_INTERNAL, uint32_t> keyBindingsInternal;

	void setKeyBindings(); // initialization function: populates keyBindings map
};

