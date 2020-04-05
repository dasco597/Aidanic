#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <map>

class Aidanic;

enum struct INPUTS {
	NONE = 0,
	ESC = (1 << 0),
	FORWARD = (1 << 1),
	BACKWARD = (1 << 2),
	LEFT = (1 << 3),
	RIGHT = (1 << 4),
	UP = (1 << 5),
	DOWN = (1 << 6),
	ROTATEL = (1 << 7),
	ROTATER = (1 << 8),
	MOUSEL = (1 << 9),
	MOUSER = (1 << 10),
	INTERACTL = (1 << 11),
	INTERACTR = (1 << 12)
}; // do not excede 32 values!

union Inputs {
	INPUTS i;
	uint32_t uint; // used for bitwise operations (one input type per bit)
	bool conatinsInput(INPUTS compValue) { return uint & static_cast<uint32_t>(compValue); }
};

class IOInterface {
public:

    void init(Aidanic* application, uint32_t width, uint32_t height);
	VkResult createVkSurface(VkInstance& instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface);
	
	int windowCloseCheck() { return glfwWindowShouldClose(window); };
	void minimizeSuspend(); // doesn't return unless window isn't minimized
	void pollEvents() { glfwPollEvents(); }; // updates glfw state (key presses etc)

	Inputs getInputs(); // returns bits corresponding to INPUTS for the different input signals
	void getMouseChange(double& mouseX, double& mouseY);
	GLFWwindow* getWindow() { return window; }

	void cleanUp();

private:

    GLFWwindow* window = nullptr;
    uint32_t windowSize[2] = { 0, 0 };

    std::map<uint32_t, Inputs> keyBindings; // <GLFW_KEY, INPUTS>
	double mousePosPrev[2] = { 0.0, 0.0 };

	void setKeyBindings(); // initialization function: populates keyBindings map
};

