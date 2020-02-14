#include "RendererRTX.h"

void RendererRTX::addDeviceExtensions() {
    deviceExtensions.push_back(VK_NV_RAY_TRACING_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME); // required by VK_NV_RAY_TRACING
    instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME); // required by VK_NV_RAY_TRACING
}

// create the storage image the ray gen shader will be writing to
void RendererRTX::createRenderImage() {
    VkImageCreateInfo imageCI = {};
    imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = swapChainImageFormat;
    imageCI.extent.width = swapChainExtent.width;
    imageCI.extent.height = swapChainExtent.height;
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CHECK_RESULT(vkCreateImage(device, &imageCI, VK_ALLOCATOR, &renderImage.image), "failed to create ray tracing storage image");
}

void RendererRTX::createScene() {
    // hard coded vertices
    struct Vertex {
        float pos[3];
    };
    std::vector<Vertex> vertices = {
        { {  1.0f,  1.0f, 0.0f } },
        { { -1.0f,  1.0f, 0.0f } },
        { {  0.0f, -1.0f, 0.0f } },
        { { -1.0f, -1.0f, 0.0f } }
    };

    // hard coded indices
    std::vector<uint32_t> indices = { 0, 1, 2, 2, 1, 3 };
    indexCount = static_cast<uint32_t>(indices.size());

    // create vertex and index buffers
}

void RendererRTX::createBottomLevelAccelerationStructure(const VkGeometryNV* geometries) {
    VkAccelerationStructureInfoNV accelerationStructureInfo{};
    accelerationStructureInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
    accelerationStructureInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
    accelerationStructureInfo.instanceCount = 0;
    accelerationStructureInfo.geometryCount = 1;
    accelerationStructureInfo.pGeometries = geometries;

    VkAccelerationStructureCreateInfoNV accelerationStructureCI{};
    accelerationStructureCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
    accelerationStructureCI.info = accelerationStructureInfo;
    VK_CHECK_RESULT(vkCreateAccelerationStructureNV(device, &accelerationStructureCI, VK_ALLOCATOR, &bottomLevelAS.accelerationStructure), "failed to create bottom level acceleration structure");

    VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo{};
    memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
    memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;
    memoryRequirementsInfo.accelerationStructure = bottomLevelAS.accelerationStructure;

    VkMemoryRequirements2 memoryRequirements{};
    vkGetAccelerationStructureMemoryRequirementsNV(device, &memoryRequirementsInfo, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, VK_ALLOCATOR, &bottomLevelAS.memory), "failed to allocate bottom level acceleration structure memory");

    VK_CHECK_RESULT(vkGetAccelerationStructureHandleNV(device, bottomLevelAS.accelerationStructure, sizeof(uint64_t), &bottomLevelAS.handle), "failed to get bottom level acceleration structure handle");
}

void RendererRTX::createTopLevelAccelerationStructure() {
    VkAccelerationStructureInfoNV accelerationStructureInfo{};
    accelerationStructureInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
    accelerationStructureInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
    accelerationStructureInfo.instanceCount = 1;
    accelerationStructureInfo.geometryCount = 0;

    VkAccelerationStructureCreateInfoNV accelerationStructureCI{};
    accelerationStructureCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
    accelerationStructureCI.info = accelerationStructureInfo;
    VK_CHECK_RESULT(vkCreateAccelerationStructureNV(device, &accelerationStructureCI, VK_ALLOCATOR, &topLevelAS.accelerationStructure), "failed to create top level acceleration stucture");

    VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo{};
    memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
    memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;
    memoryRequirementsInfo.accelerationStructure = topLevelAS.accelerationStructure;

    VkMemoryRequirements2 memoryRequirements{};
    vkGetAccelerationStructureMemoryRequirementsNV(device, &memoryRequirementsInfo, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, VK_ALLOCATOR, &topLevelAS.memory), "failed to allocate top level acceleration structure memory");

    VK_CHECK_RESULT(vkGetAccelerationStructureHandleNV(device, topLevelAS.accelerationStructure, sizeof(uint64_t), &topLevelAS.handle), "failed to get top level acceleration structure handle");
}