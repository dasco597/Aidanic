#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <map>
#include <vector>

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

namespace IOInterface {

	// public functions

    void init(std::vector<const char*>& requiredExtensions, uint32_t width, uint32_t height);
	void glfwErrorCallback(int error, const char* errorMessage);
	VkResult createVkSurface(VkInstance& instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface);

	std::array<int, 2> getWindowSize();

	int windowCloseCheck();
	void minimizeSuspend(); // doesn't return unless window isn't minimized
	void pollEvents(); // updates glfw state (key presses etc)

	void updateImGui();
	Inputs getInputs();
	std::array<double, 2> getMouseChange();

	void cleanUp();

};

