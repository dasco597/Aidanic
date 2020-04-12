#pragma once

#include "tools/Log.h"
#include "Model.h"

#include <glm.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <string>
#include <vector>
#include <optional>

namespace Vk {
    std::string _errorString(VkResult res);
}

// Check vulkan result macro
#define VK_CHECK_RESULT(result, ...) if (result != VK_SUCCESS) { AID_TRACE("VkResult = {}", Vk::_errorString(result)); AID_ERROR(__VA_ARGS__); }

#define DEFAULT_FENCE_TIMEOUT 100000000000
#define VK_ALLOCATOR nullptr

namespace Vk {
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily, computeFamily, presentFamily;

        bool isComplete() {
            return graphicsFamily.has_value() && computeFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct Buffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = static_cast<VkDeviceSize>(0);
        VkDeviceSize dynamicStride = 0;
    };

    struct StorageImage {
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFormat format;
        VkExtent2D extent;

        void destroy(VkDevice device);
    };

    struct AccelerationStructure {
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkAccelerationStructureNV accelerationStructure = VK_NULL_HANDLE;
        uint64_t handle = UINT64_MAX;
    };

    struct BLASInstance {
        glm::mat3x4 transform;
        uint32_t instanceId : 24;
        uint32_t mask : 8;
        uint32_t instanceOffset : 24;
        uint32_t flags : 8;
        uint64_t accelerationStructureHandle;
    };

    struct AABB {
        float aabb_minx;
        float aabb_miny;
        float aabb_minz;
        float aabb_maxx;
        float aabb_maxy;
        float aabb_maxz;

        AABB() {}
        AABB(Model::Sphere sphere);
    };

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);
    void endSingleTimeCommands(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool commandPool);

    std::vector<char> readFile(const std::string filename);
    VkPipelineShaderStageCreateInfo loadShader(VkDevice device, const std::string filename, VkShaderStageFlagBits stage, VkShaderModule& shaderModuleWriteOut);
    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
}