#pragma once

#include "tools/VkHelper.h"
#include "Model.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <vector>
#include <string>

// also defined in sphere.rint
#define SPHERE_COUNT_PER_TLAS 8

namespace Renderer {

    // public functions declarations

    void init(std::vector<const char*>& requiredExtensions, glm::mat4 viewInverse, glm::mat4 projInverse, glm::vec3 cameraPos);
    void drawFrame(bool framebufferResized, glm::mat4 viewInverse, glm::mat4 projInverse, glm::vec3 cameraPos, bool renderImGui = false);
    void cleanUp();

    int addEllipsoid(Model::Ellipsoid ellipsoid); // returns 0 for success

    VkDevice getDevice();
    VkPhysicalDevice getPhysicalDevice();
    VkQueue getGraphicsQueue();
    VkSurfaceKHR getSurface();
    VkCommandPool getCommandPool();
    uint32_t getNumSwapchainImages();
    Vk::StorageImage getRenderImage();
};

