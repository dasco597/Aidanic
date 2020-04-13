#include "VkHelper.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <vulkan/vulkan_core.h>

#include <iostream>
#include <fstream>

using namespace Vk;

#define AABB_EDGE_FACTOR 1.1

std::string Vk::_errorString(VkResult res)
{
    switch (res)
    {
#define STR(r) \
        case VK_##r: return #r
        STR(NOT_READY);
        STR(TIMEOUT);
        STR(EVENT_SET);
        STR(EVENT_RESET);
        STR(INCOMPLETE);
        STR(ERROR_OUT_OF_HOST_MEMORY);
        STR(ERROR_OUT_OF_DEVICE_MEMORY);
        STR(ERROR_INITIALIZATION_FAILED);
        STR(ERROR_DEVICE_LOST);
        STR(ERROR_MEMORY_MAP_FAILED);
        STR(ERROR_LAYER_NOT_PRESENT);
        STR(ERROR_EXTENSION_NOT_PRESENT);
        STR(ERROR_FEATURE_NOT_PRESENT);
        STR(ERROR_INCOMPATIBLE_DRIVER);
        STR(ERROR_TOO_MANY_OBJECTS);
        STR(ERROR_FORMAT_NOT_SUPPORTED);
        STR(ERROR_SURFACE_LOST_KHR);
        STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
        STR(SUBOPTIMAL_KHR);
        STR(ERROR_OUT_OF_DATE_KHR);
        STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
        STR(ERROR_VALIDATION_FAILED_EXT);
        STR(ERROR_INVALID_SHADER_NV);
#undef STR
    default:
        return std::to_string(res);
    }
}

Vk::AABB::AABB(Model::Sphere sphere) {
    float edge = sphere.posRadius.w * AABB_EDGE_FACTOR;
    aabb_minx = sphere.posRadius.x - sphere.posRadius.w - edge;
    aabb_miny = sphere.posRadius.y - sphere.posRadius.w - edge;
    aabb_minz = sphere.posRadius.z - sphere.posRadius.w - edge;
    aabb_maxx = sphere.posRadius.x + sphere.posRadius.w + edge;
    aabb_maxy = sphere.posRadius.y + sphere.posRadius.w + edge;
    aabb_maxz = sphere.posRadius.z + sphere.posRadius.w + edge;
}

SwapChainSupportDetails Vk::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    Vk::SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

QueueFamilyIndices Vk::findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const VkQueueFamilyProperties& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphicsFamily = i;

        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
            indices.computeFamily = i;

        //if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
        //    indices.transferFamily = i;

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport)
            indices.presentFamily = i;

        if (indices.isComplete()) break;

        i++;
    }

    return indices;
}

VkShaderModule Vk::createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        AID_ERROR("failed to create shader module!");
    }

    return shaderModule;
}

uint32_t Vk::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    AID_ERROR("failed to find suitable memory type!");
}

void _BufferCommon::createCommon(VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkDeviceSize size, VkDevice device, VkPhysicalDevice physicalDevice) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK_RESULT(vkCreateBuffer(device, &bufferInfo, VK_ALLOCATOR, &buffer), "failed to create buffer!");

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    VK_CHECK_RESULT(vkAllocateMemory(device, &allocInfo, VK_ALLOCATOR, &memory), "failed to allocate buffer memory!");

    vkBindBufferMemory(device, buffer, memory, 0);
    this->size = size;
}

void _BufferCommon::destroy(VkDevice device) {
    if (buffer != VK_NULL_HANDLE) vkDestroyBuffer(device, buffer, VK_ALLOCATOR);
    if (memory != VK_NULL_HANDLE) vkFreeMemory(device, memory, VK_ALLOCATOR);
}

void BufferDeviceLocal::create(VkBufferUsageFlags usage, VkDeviceSize size, VkDevice device, VkPhysicalDevice physicalDevice) {
    createCommon(usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, size, device, physicalDevice);
}

void BufferDeviceLocal::upload(void* data, VkDeviceSize size, VkDeviceSize bufferOffset, VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, VkCommandPool commandPool) {
    BufferHostVisible stagingBuffer;
    stagingBuffer.create(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, device, physicalDevice);
    stagingBuffer.upload(data, size, 0, device);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = bufferOffset;

    VkCommandBuffer commandBuffer = Vk::beginSingleTimeCommands(device, commandPool);
    vkCmdCopyBuffer(commandBuffer, stagingBuffer.buffer, buffer, 1, &copyRegion);
    Vk::endSingleTimeCommands(device, commandBuffer, queue, commandPool);

    stagingBuffer.destroy(device);
}

void BufferHostVisible::create(VkBufferUsageFlags usage, VkDeviceSize size, VkDevice device, VkPhysicalDevice physicalDevice) {
    createCommon(usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, size, device, physicalDevice);
}

void BufferHostVisible::upload(void* data, VkDeviceSize size, VkDeviceSize bufferOffset, VkDevice device) {
    if (size + bufferOffset > this->size) {
        AID_ERROR("uploadBufferHostVisible: trying to upload outside of buffer memory");
    }

    void* dataDst;
    VK_CHECK_RESULT(vkMapMemory(device, memory, bufferOffset, size, 0, &dataDst), "failed to map buffer memory");
    memcpy(dataDst, data, (size_t)size);
    vkUnmapMemory(device, memory);
}

VkCommandBuffer Vk::beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool) {
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer), "failed to allocate single time commmand buffer");

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void Vk::endSingleTimeCommands(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool commandPool) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE), "failed to submit single time command buffer");
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void StorageImage::destroy(VkDevice device) {
    vkFreeMemory(device, memory, VK_ALLOCATOR);
    vkDestroyImageView(device, view, VK_ALLOCATOR);
    vkDestroyImage(device, image, VK_ALLOCATOR);
}

void Texture::create(std::string path) {
    //Buffer stagingBuffer;
    //create
    //
    //VkImageCreateInfo imageInfo = {};
    //imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    //imageInfo.imageType = VK_IMAGE_TYPE_2D;
    //imageInfo.extent.width = static_cast<uint32_t>(texWidth);
    //imageInfo.extent.height = static_cast<uint32_t>(texHeight);
    //imageInfo.extent.depth = 1;
    //imageInfo.mipLevels = 1;
    //imageInfo.arrayLayers = 1;
}

void Texture::destroy(VkDevice device) {
    vkFreeMemory(device, memory, VK_ALLOCATOR);
    vkDestroyImage(device, image, VK_ALLOCATOR);
}

std::vector<char> Vk::readFile(const std::string filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        AID_ERROR("failed to open file: " + filename);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

VkPipelineShaderStageCreateInfo Vk::loadShader(VkDevice device, const std::string filename, VkShaderStageFlagBits stage, VkShaderModule& shaderModuleWriteOut) {
    auto shaderCode = readFile(filename);
    shaderModuleWriteOut = createShaderModule(device, shaderCode);

    VkPipelineShaderStageCreateInfo shaderStageInfo = {};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = stage;
    shaderStageInfo.module = shaderModuleWriteOut;
    shaderStageInfo.pName = "main";
    return shaderStageInfo;
}
