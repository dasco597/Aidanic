#pragma once

#include "tools/Log.h"

#include <glm.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <vector>
#include <optional>

// todo remove references from vk objects

// Check vulkan result macro
#define VK_CHECK_RESULT(result, ...) if (result != VK_SUCCESS) { AID_TRACE("vkResult = {}", result); AID_ERROR(__VA_ARGS__); }

#define DEFAULT_FENCE_TIMEOUT 100000000000
#define VK_ALLOCATOR nullptr

struct Vertex { glm::vec3 pos; };
struct Model {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

namespace Vk {
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct Buffer {
        VkBuffer buffer;
        VkDeviceMemory memory;
        VkDeviceSize size = static_cast<VkDeviceSize>(0);
        VkDeviceSize dynamicStride = 0;
    };

    struct StorageImage {
        VkDeviceMemory memory;
        VkImage image;
        VkImageView view;
        VkFormat format;
        VkExtent2D extent;

        void destroy(VkDevice device);
    };

    struct AccelerationStructure {
        VkDeviceMemory memory;
        VkAccelerationStructureNV accelerationStructure;
        uint64_t handle;
    };

    struct BLASInstance {
        glm::mat3x4 transform;
        uint32_t instanceId : 24;
        uint32_t mask : 8;
        uint32_t instanceOffset : 24;
        uint32_t flags : 8;
        uint64_t accelerationStructureHandle;
    };

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice& device, VkSurfaceKHR& surface);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice& device, VkSurfaceKHR& surface);
    uint32_t findMemoryType(VkPhysicalDevice& physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    VkCommandBuffer beginSingleTimeCommands(VkDevice& device, VkCommandPool& commandPool);
    void endSingleTimeCommands(VkDevice& device, VkCommandBuffer commandBuffer, VkQueue& queue, VkCommandPool& commandPool);

    std::vector<char> readFile(const std::string filename);
    VkPipelineShaderStageCreateInfo loadShader(VkDevice& device, const std::string filename, VkShaderStageFlagBits stage, VkShaderModule& shaderModuleWriteOut);
    VkShaderModule createShaderModule(VkDevice& device, const std::vector<char>& code);
};