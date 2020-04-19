#pragma once

#include "tools/VkHelper.h"
#include <glm.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace ImGuiVk {

    // public functions

    void init();

    void recordRenderCommands(uint32_t swapchainIndex);
    void recreateFramebuffer();

    float* getpClearValue();
    VkCommandBuffer getCommandBuffer(uint32_t swapchainIndex);
    bool shouldRender(uint32_t swapchainIndex);

    void cleanup();

};
