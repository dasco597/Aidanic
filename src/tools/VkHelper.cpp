#include "VkHelper.h"
#include "Renderer.h"

using namespace VkHelper;
using namespace DOD;

#define VALID_ID_CHECK(bufferID, vec) if (bufferID.isInvalid() || vec.size() <= bufferID.id) { AID_WARN("invalid buffer id!"); return; }

// BUFFER CREATION

BufferIDDeviceLocal BufferController::createBufferDeviceLocal(VkBufferUsageFlags usage, VkDeviceSize size) {
    BufferDataDeviceLocal newBuffer;
    createBufferCommon(newBuffer, usage, size, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    BufferIDDeviceLocal id(buffersDeviceLocal.size());
    newBuffer.bufferID = id;
    buffersDeviceLocal.push_back(newBuffer);
    return id;
}

BufferIDHostVisible BufferController::createBufferHostVisible(VkBufferUsageFlags usage, VkDeviceSize size) {
    BufferDataHostVisible newBuffer;
    createBufferCommon(newBuffer, usage, size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    BufferIDHostVisible id(buffersHostVisible.size());
    newBuffer.bufferID = id;
    buffersHostVisible.push_back(newBuffer);
    return id;
}

inline VkResult BufferController::createBufferCommon(BufferDataCommon& pBuffer, VkBufferUsageFlags usage, VkDeviceSize size, VkMemoryPropertyFlags properties) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK_RESULT(vkCreateBuffer(pRenderer->device, &bufferInfo, pAllocator, &pBuffer.buffer), "failed to create buffer!");

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(pRenderer->device, pBuffer.buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(pRenderer->physicalDevice, memRequirements.memoryTypeBits, properties);

    VK_CHECK_RESULT(vkAllocateMemory(pRenderer->device, &allocInfo, pAllocator, &pBuffer.memory), "failed to allocate buffer memory!");

    vkBindBufferMemory(pRenderer->device, pBuffer.buffer, pBuffer.memory, 0);
    pBuffer.size = size;
}

uint32_t VkHelper::findMemoryType(VkPhysicalDevice& physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    AID_ERROR("failed to find suitable memory type!");
}

// BUFFER DATA UPLOAD

void BufferController::uploadBuffer(BufferIDDeviceLocal bufferID, void* data, VkDeviceSize size) {
    VALID_ID_CHECK(bufferID, buffersDeviceLocal)

    BufferDataHostVisible stagingBuffer;
    createBufferCommon(stagingBuffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    uploadBufferHostVisible(stagingBuffer, data, size);

    VkBufferCopy copyRegion = {};
    copyRegion.size = size;
    VkCommandBuffer commandBuffer = pRenderer->beginSingleTimeCommands();
    vkCmdCopyBuffer(commandBuffer, stagingBuffer.buffer, buffersDeviceLocal[bufferID.id].buffer, 1, &copyRegion);
    pRenderer->endSingleTimeCommands(commandBuffer);

    cleanUpBufferCommon(stagingBuffer, stagingBuffer.bufferID);
}

void BufferController::uploadBuffer(BufferIDHostVisible bufferID, void* data, VkDeviceSize size) {
    VALID_ID_CHECK(bufferID, buffersHostVisible)
    uploadBufferHostVisible(buffersHostVisible[bufferID.id], data, size);
}

void BufferController::uploadBufferHostVisible(BufferDataHostVisible& buffer, void* data, VkDeviceSize size) {
    void* dataDst;
    VK_CHECK_RESULT(vkMapMemory(pRenderer->device, buffer.memory, 0, size, 0, &dataDst), "failed to map buffer memory");
    memcpy(dataDst, data, (size_t)size);
    vkUnmapMemory(pRenderer->device, buffer.memory);
}

// BUFFER CLEAN-UP

void BufferController::destroyBuffer(BufferIDDeviceLocal bufferID) {
    cleanUpBufferCommon(buffersDeviceLocal[bufferID.id], buffersDeviceLocal[bufferID.id].bufferID);
    buffersDeviceLocal[bufferID.id].bufferID.invalidate();
}

void BufferController::destroyBuffer(BufferIDHostVisible bufferID) {
    cleanUpBufferCommon(buffersHostVisible[bufferID.id], buffersHostVisible[bufferID.id].bufferID);
    buffersHostVisible[bufferID.id].bufferID.invalidate();
}

void BufferController::cleanUpBuffers() {

    for (BufferDataDeviceLocal& buffer : buffersDeviceLocal)
        cleanUpBufferCommon(buffer, buffer.bufferID);
    buffersDeviceLocal.clear();

    for (BufferDataHostVisible& buffer : buffersHostVisible)
        cleanUpBufferCommon(buffer, buffer.bufferID);
    buffersHostVisible.clear();
}

inline void BufferController::cleanUpBufferCommon(BufferDataCommon& pBuffer, DOD::ObjectID& objectID) {
    if (!objectID.isInvalid()) {
        vkDestroyBuffer(pRenderer->device, pBuffer.buffer, pAllocator);
        vkFreeMemory(pRenderer->device, pBuffer.memory, pAllocator);
    }
}
