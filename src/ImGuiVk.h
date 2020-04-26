#pragma once

#include "tools/VkHelper.h"
#include <glm.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace ImGuiVk {

    // public functions

    void init();

    void recordRenderCommands(uint32_t frame);
    void recreateFramebuffers();

    float* getpClearValue();
    VkCommandBuffer getCommandBuffer(uint32_t frame);
    bool shouldRender(uint32_t frame);

    void cleanup();

};
