#include "RendererRTX.h"

#include <vulkan/vulkan_core.h>

// todo replace nullptr with VK_ALLOCATOR

// hard coded triangle TODO: other vertex attributes
struct Vertex {
    float pos[3];
};
std::vector<Vertex> vertices = {
    { {  1.0f,  1.0f, 0.0f } },
    { { -1.0f,  1.0f, 0.0f } },
    { {  0.0f, -1.0f, 0.0f } },
    { { -1.0f, -1.0f, 0.0f } }
};
std::vector<uint32_t> indices = { 0, 1, 2, 2, 1, 3 };

// shader indices
#define INDEX_RAYGEN 0
#define INDEX_MISS 1
#define INDEX_CLOSEST_HIT 2

// INITIALIZATION

void RendererRTX::Init(IOInterface* ioInterface) {
    RendererBase::init(ioInterface);

    createScene();
    createRenderImage();
    createUniformBuffer();
    createRayTracingPipeline();
    createShaderBindingTable();
    createDescriptorSets();
    buildCommandBuffers();

    initialized = true;
}

void RendererRTX::addDeviceExtensions() {
    deviceExtensions.push_back(VK_NV_RAY_TRACING_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME); // required by VK_NV_RAY_TRACING
    instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME); // required by VK_NV_RAY_TRACING
}

void RendererRTX::createScene() {
    // create vertex and index buffers

    createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBuffer, vertexBufferMemory, sizeof(vertices[0]) * vertices.size(), vertices.data());
    createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBuffer, indexBufferMemory, sizeof(indices[0]) * indices.size(), indices.data());

    // create a bottom-level acceleration structure containing the scene geometry

    VkGeometryNV geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NV;
    geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
    geometry.geometry.triangles.vertexData = vertexBuffer;
    geometry.geometry.triangles.vertexOffset = 0;
    geometry.geometry.triangles.vertexCount = static_cast<uint32_t>(vertices.size());
    geometry.geometry.triangles.vertexStride = sizeof(Vertex);
    geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometry.geometry.triangles.indexData = indexBuffer;
    geometry.geometry.triangles.indexOffset = 0;
    geometry.geometry.triangles.indexCount = indexCount;
    geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    geometry.geometry.triangles.transformData = VK_NULL_HANDLE;
    geometry.geometry.triangles.transformOffset = 0;
    geometry.geometry.aabbs = {};
    geometry.geometry.aabbs.sType = { VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV };
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_NV;

    createBottomLevelAccelerationStructure(&geometry);

    // create the top-level acceleration structure containing BLAS instance information

    VkBuffer instanceBuffer;
    VkDeviceMemory instanceMemory;
    glm::mat3x4 transform = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };

    BLASInstance geometryInstance{};
    geometryInstance.transform = transform;
    geometryInstance.instanceId = 0;
    geometryInstance.mask = 0xff;
    geometryInstance.instanceOffset = 0;
    geometryInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;
    geometryInstance.accelerationStructureHandle = bottomLevelAS.handle;

    createBuffer(VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        instanceBuffer, instanceMemory, sizeof(BLASInstance), &geometryInstance);

    createTopLevelAccelerationStructure();

    // acceleration structure building requires some scratch space to store temporary information

    VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo{};
    memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
    memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;

    VkMemoryRequirements2 memReqBottomLevelAS;
    memoryRequirementsInfo.accelerationStructure = bottomLevelAS.accelerationStructure;
    vkGetAccelerationStructureMemoryRequirementsNV(device, &memoryRequirementsInfo, &memReqBottomLevelAS);

    VkMemoryRequirements2 memReqTopLevelAS;
    memoryRequirementsInfo.accelerationStructure = topLevelAS.accelerationStructure;
    vkGetAccelerationStructureMemoryRequirementsNV(device, &memoryRequirementsInfo, &memReqTopLevelAS);

    const VkDeviceSize scratchBufferSize = std::max(memReqBottomLevelAS.memoryRequirements.size, memReqTopLevelAS.memoryRequirements.size);

    VkBuffer scratchBuffer;
    VkDeviceMemory scratchMemory;
    createBuffer(VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory, scratchBufferSize);

    VkCommandBuffer cmdBuffer = beginSingleTimeCommands();

    // build bottom-level acceleration structure

    VkAccelerationStructureInfoNV buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    vkCmdBuildAccelerationStructureNV(
        cmdBuffer,
        &buildInfo,
        VK_NULL_HANDLE,
        0,
        VK_FALSE,
        bottomLevelAS.accelerationStructure,
        VK_NULL_HANDLE,
        scratchBuffer,
        0);

    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
    memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 0, 1, &memoryBarrier, 0, 0, 0, 0);

    // build top-level acceleration structure

    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
    buildInfo.pGeometries = 0;
    buildInfo.geometryCount = 0;
    buildInfo.instanceCount = 1;

    vkCmdBuildAccelerationStructureNV(
        cmdBuffer,
        &buildInfo,
        instanceBuffer,
        0,
        VK_FALSE,
        topLevelAS.accelerationStructure,
        VK_NULL_HANDLE,
        scratchBuffer,
        0);

    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 0, 1, &memoryBarrier, 0, 0, 0, 0);

    endSingleTimeCommands(cmdBuffer);

    vkDestroyBuffer(device, scratchBuffer, VK_ALLOCATOR);
    vkDestroyBuffer(device, instanceBuffer, VK_ALLOCATOR);
    vkFreeMemory(device, scratchMemory, VK_ALLOCATOR);
    vkFreeMemory(device, instanceMemory, VK_ALLOCATOR);
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

void RendererRTX::createUniformBuffer() {
    createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        ubo.buffer, ubo.memory, sizeof(uniformData), &uniformData);
    VK_CHECK_RESULT(vkMapMemory(device, ubo.memory, 0, VK_WHOLE_SIZE, 0, &ubo.mapping), "failed to map ubo memory");
}

void RendererRTX::createRayTracingPipeline() {
    VkDescriptorSetLayoutBinding layoutBindingAccelerationStructure {};
    layoutBindingAccelerationStructure.binding = 0;
    layoutBindingAccelerationStructure.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
    layoutBindingAccelerationStructure.descriptorCount = 1;
    layoutBindingAccelerationStructure.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;

    VkDescriptorSetLayoutBinding layoutBindingRenderImage {};
    layoutBindingRenderImage.binding = 1;
    layoutBindingRenderImage.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    layoutBindingRenderImage.descriptorCount = 1;
    layoutBindingRenderImage.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;

    VkDescriptorSetLayoutBinding layoutBindingUniformBuffer {};
    layoutBindingUniformBuffer.binding = 2;
    layoutBindingUniformBuffer.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBindingUniformBuffer.descriptorCount = 1;
    layoutBindingUniformBuffer.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;

    std::vector<VkDescriptorSetLayoutBinding> bindings({
        layoutBindingAccelerationStructure,
        layoutBindingRenderImage,
        layoutBindingUniformBuffer });

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI {};
    descriptorSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCI.bindingCount = static_cast<uint32_t>(bindings.size());
    descriptorSetLayoutCI.pBindings = bindings.data();
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, VK_ALLOCATOR, &descriptorSetLayout), "failed to create descriptor set layout");

    VkPipelineLayoutCreateInfo pipelineLayoutCI {};
    pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCI.setLayoutCount = 1;
    pipelineLayoutCI.pSetLayouts = &descriptorSetLayout;
    vkCreatePipelineLayout(device, &pipelineLayoutCI, VK_ALLOCATOR, &pipelineLayout);

    std::array<VkPipelineShaderStageCreateInfo, 3> shaderStages;
    shaderStages[INDEX_RAYGEN] =      loadShader(std::string(AIDANIC_CONFIG::getAssetPath()) + "spirv/raygen.rgen.spv",      VK_SHADER_STAGE_RAYGEN_BIT_NV);
    shaderStages[INDEX_MISS] =        loadShader(std::string(AIDANIC_CONFIG::getAssetPath()) + "spirv/miss.rmiss.spv",       VK_SHADER_STAGE_MISS_BIT_NV);
    shaderStages[INDEX_CLOSEST_HIT] = loadShader(std::string(AIDANIC_CONFIG::getAssetPath()) + "spirv/closesthit.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);


}

// HELPER FUNCTIONS

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

