#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <map>

class Aidanic;

enum struct INPUTS {
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
	INTERACTR	= (1 << 12)
};

class IOInterface {
public:
    void Init(Aidanic* application, uint32_t width, uint32_t height);
	VkResult CreateWindowSurface(VkInstance& instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface);
	
	inline int WindowCloseCheck() { return glfwWindowShouldClose(window); };
	void MinimizeSuspend(); // doesn't return unless window isn't minimized
	inline void PollEvents() { glfwPollEvents(); }; // updates glfw state (key presses etc)
	uint32_t GetInputs(); // returns bits corresponding to INPUTS for the different input signals

	void CleanUp();

private:

    GLFWwindow* window = nullptr;
    uint32_t windowSize[2] = { 0, 0 };

    std::map <uint32_t, INPUTS> keyBindings;
	double mousePosPrev[2] = { 0.0, 0.0 };

	void SetKeyBindings(); // initialization function: populates keyBindings map
};

