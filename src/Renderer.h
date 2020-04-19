#pragma once

#include "Model.h"
#include "tools/VkHelper.h"
#include "vulkan/vulkan.h"
#include <vector>

// also defined in sphere.rint
#define SPHERE_COUNT_PER_TLAS 8

namespace Renderer {

    // public functions declarations

    void init(std::vector<const char*>& requiredExtensions, glm::mat4 viewInverse, glm::mat4 projInverse, glm::vec3 cameraPos);
    void drawFrame(bool framebufferResized, glm::mat4 viewInverse, glm::mat4 projInverse, glm::vec3 cameraPos, bool renderImGui = false);
    void cleanUp();

    int addEllipsoid(Model::EllipsoidID ellipsoidID); // returns 0 for success

    VkDevice getDevice();
    VkPhysicalDevice getPhysicalDevice();
    VkQueue getGraphicsQueue();
    VkSurfaceKHR getSurface();
    VkCommandPool getCommandPool();
    uint32_t getNumSwapchainImages();
    Vk::StorageImage getRenderImage();
};

