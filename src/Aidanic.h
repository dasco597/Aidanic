#pragma once

#include "IOInterface.h"
#include "Renderer.h"
#include "ImGuiVk.h"
#include "tools/config.h"

#include <imgui.h>
#include <glm.hpp>
#include <atomic>

namespace Aidanic {
    
    // public function declarations

    VkResult createVkSurface(VkInstance& instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface);
    std::array<int, 2> getWindowSize();
    void setWindowResizedFlag();

};
