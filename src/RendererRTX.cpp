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
enum {
    INDEX_RAYGEN,
    INDEX_MISS,
    INDEX_CLOSEST_HIT,
    SHADER_COUNT
};

// shader files (relative to assets folder, AIDANIC_CONFIG::getAssetsPath())
#define SHADER_RAYGEN "spirv/raygen.rgen.spv"
#define SHADER_MISS "spirv/miss.rmiss.spv"
#define SHADER_CLOSEST_HIT "spirv/closesthit.rchit.spv"

// INITIALIZATION

void RendererRTX::Init(IOInterface* ioInterface) {
    // vulkan initialization
    RendererBase::initBase(ioInterface);

    // Query the ray tracing properties of the current implementation
    rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;
    VkPhysicalDeviceProperties2 deviceProps2{};
    deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProps2.pNext = &rayTracingProperties;
    vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProps2);

    // Get VK_NV_ray_tracing related function pointers
    vkCreateAccelerationStructureNV = reinterpret_cast<PFN_vkCreateAccelerationStructureNV>(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureNV"));
    vkDestroyAccelerationStructureNV = reinterpret_cast<PFN_vkDestroyAccelerationStructureNV>(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureNV"));
    vkBindAccelerationStructureMemoryNV = reinterpret_cast<PFN_vkBindAccelerationStructureMemoryNV>(vkGetDeviceProcAddr(device, "vkBindAccelerationStructureMemoryNV"));
    vkGetAccelerationStructureHandleNV = reinterpret_cast<PFN_vkGetAccelerationStructureHandleNV>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureHandleNV"));
    vkGetAccelerationStructureMemoryRequirementsNV = reinterpret_cast<PFN_vkGetAccelerationStructureMemoryRequirementsNV>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureMemoryRequirementsNV"));
    vkCmdBuildAccelerationStructureNV = reinterpret_cast<PFN_vkCmdBuildAccelerationStructureNV>(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructureNV"));
    vkCreateRayTracingPipelinesNV = reinterpret_cast<PFN_vkCreateRayTracingPipelinesNV>(vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesNV"));
    vkGetRayTracingShaderGroupHandlesNV = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesNV>(vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesNV"));
    vkCmdTraceRaysNV = reinterpret_cast<PFN_vkCmdTraceRaysNV>(vkGetDeviceProcAddr(device, "vkCmdTraceRaysNV"));

    // ray tracing initialization
    createScene();
    createRenderImage();
    createUniformBuffer();
    createRayTracingPipeline();
    createShaderBindingTable();
    createDescriptorSets();
    buildCommandBuffers();

    _initialized = true;
}

void RendererRTX::addDeviceExtensions() {
    deviceExtensions.push_back(VK_NV_RAY_TRACING_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME); // required by VK_NV_RAY_TRACING
    instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME); // required by VK_NV_RAY_TRACING
}

void RendererRTX::createScene() {
    // create vertex and index buffers

    createBuffer(vertexBuffer, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sizeof(vertices[0]) * vertices.size());
    uploadBuffer(vertexBuffer, sizeof(vertices[0]) * vertices.size(), vertices.data());
    createBuffer(indexBuffer, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sizeof(indices[0]) * indices.size());
    uploadBuffer(indexBuffer, sizeof(indices[0]) * indices.size(), indices.data());

    // create a bottom-level acceleration structure containing the scene geometry

    VkGeometryNV geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NV;
    geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
    geometry.geometry.triangles.vertexData = vertexBuffer.buffer;
    geometry.geometry.triangles.vertexOffset = 0;
    geometry.geometry.triangles.vertexCount = static_cast<uint32_t>(vertices.size());
    geometry.geometry.triangles.vertexStride = sizeof(Vertex);
    geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometry.geometry.triangles.indexData = indexBuffer.buffer;
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

    Buffer instanceBuffer;
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

    createBuffer(instanceBuffer, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(BLASInstance));
    uploadBuffer(instanceBuffer, sizeof(BLASInstance), &geometryInstance);

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

    Buffer scratchBuffer;
    createBuffer(scratchBuffer, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBufferSize);

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
        scratchBuffer.buffer,
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
        instanceBuffer.buffer,
        0,
        VK_FALSE,
        topLevelAS.accelerationStructure,
        VK_NULL_HANDLE,
        scratchBuffer.buffer,
        0);

    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 0, 1, &memoryBarrier, 0, 0, 0, 0);

    endSingleTimeCommands(cmdBuffer);

    vkDestroyBuffer(device, scratchBuffer.buffer, VK_ALLOCATOR);
    vkDestroyBuffer(device, instanceBuffer.buffer, VK_ALLOCATOR);
    vkFreeMemory(device, scratchBuffer.memory, VK_ALLOCATOR);
    vkFreeMemory(device, instanceBuffer.memory, VK_ALLOCATOR);
}

void RendererRTX::createRenderImage() {
    VkImageCreateInfo imageCI = {};
    imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = swapchain.imageFormat;
    imageCI.extent.width = swapchain.extent.width;
    imageCI.extent.height = swapchain.extent.height;
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CHECK_RESULT(vkCreateImage(device, &imageCI, VK_ALLOCATOR, &renderImage.image), "failed to create ray tracing storage image");
}

void RendererRTX::createUniformBuffer() {
    createBuffer(ubo, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(uniformData));
    uploadBuffer(ubo, sizeof(uniformData), &uniformData);
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

    std::array<VkPipelineShaderStageCreateInfo, SHADER_COUNT> shaderStages;
    shaderStages[INDEX_RAYGEN] =      loadShader(std::string(AIDANIC_CONFIG::getAssetPath()) + std::string(SHADER_RAYGEN),      VK_SHADER_STAGE_RAYGEN_BIT_NV);
    shaderStages[INDEX_MISS] =        loadShader(std::string(AIDANIC_CONFIG::getAssetPath()) + std::string(SHADER_MISS),       VK_SHADER_STAGE_MISS_BIT_NV);
    shaderStages[INDEX_CLOSEST_HIT] = loadShader(std::string(AIDANIC_CONFIG::getAssetPath()) + std::string(SHADER_CLOSEST_HIT), VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);

    // ray tracing shader groups
    std::array<VkRayTracingShaderGroupCreateInfoNV, SHADER_COUNT> shaderGroups{};
    for (auto& group : shaderGroups) {
        group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV;
        group.generalShader = VK_SHADER_UNUSED_NV;
        group.closestHitShader = VK_SHADER_UNUSED_NV;
        group.anyHitShader = VK_SHADER_UNUSED_NV;
        group.intersectionShader = VK_SHADER_UNUSED_NV;
    }

    shaderGroups[INDEX_RAYGEN].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
    shaderGroups[INDEX_RAYGEN].generalShader = INDEX_RAYGEN;
    shaderGroups[INDEX_MISS].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
    shaderGroups[INDEX_MISS].generalShader = INDEX_MISS;
    shaderGroups[INDEX_CLOSEST_HIT].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV;
    shaderGroups[INDEX_CLOSEST_HIT].generalShader = VK_SHADER_UNUSED_NV;
    shaderGroups[INDEX_CLOSEST_HIT].closestHitShader = INDEX_CLOSEST_HIT;

    VkRayTracingPipelineCreateInfoNV rayPipelineCI{};
    rayPipelineCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV;
    rayPipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    rayPipelineCI.pStages = shaderStages.data();
    rayPipelineCI.groupCount = static_cast<uint32_t>(shaderGroups.size());
    rayPipelineCI.pGroups = shaderGroups.data();
    rayPipelineCI.maxRecursionDepth = 1;
    rayPipelineCI.layout = pipelineLayout;
    VK_CHECK_RESULT(vkCreateRayTracingPipelinesNV(device, VK_NULL_HANDLE, 1, &rayPipelineCI, VK_ALLOCATOR, &pipeline), "failed to create ray tracing pipeline");
}

void RendererRTX::createShaderBindingTable() {
    // create buffer for shader binding table
    const uint32_t shaderGroupHandleSize = rayTracingProperties.shaderGroupHandleSize;
    const uint32_t sbtSize = shaderGroupHandleSize * SHADER_COUNT;
    createBuffer(shaderBindingTable, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, sbtSize);
    vkMapMemory(device, shaderBindingTable.memory, 0, sbtSize, 0, &shaderBindingTable.mapping);

    auto shaderHandleStorage = new uint8_t[sbtSize];
    // get shader identifiers
    VK_CHECK_RESULT(vkGetRayTracingShaderGroupHandlesNV(device, pipeline, 0, SHADER_COUNT, sbtSize, shaderHandleStorage), "failed to get shader group handles");
    auto* data = static_cast<uint8_t*>(shaderBindingTable.mapping);
    // Copy the shader identifiers to the shader binding table
    for (int i = 0; i < SHADER_COUNT; i++) {
        memcpy(data, shaderHandleStorage + i * shaderGroupHandleSize, shaderGroupHandleSize);
        data += shaderGroupHandleSize;
    }
    vkUnmapMemory(device, shaderBindingTable.memory);
}

void RendererRTX::createDescriptorSets() {
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
    };

    VkDescriptorPoolCreateInfo descriptorPoolCI{};
    descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolCI.pPoolSizes = poolSizes.data();
    descriptorPoolCI.maxSets = 1;
    VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCI, VK_ALLOCATOR, &descriptorPool), "failed to create descriptor pool");

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;
    vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet);

    // acceleration structure descriptor

    VkWriteDescriptorSetAccelerationStructureNV descriptorAccelerationStructureInfo{};
    descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV;
    descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
    descriptorAccelerationStructureInfo.pAccelerationStructures = &topLevelAS.accelerationStructure;

    VkWriteDescriptorSet accelerationStructureWrite{};
    accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    // The specialized acceleration structure descriptor has to be chained
    accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
    accelerationStructureWrite.dstSet = descriptorSet;
    accelerationStructureWrite.descriptorCount = 1;
    accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
    accelerationStructureWrite.dstBinding = 0;

    // render image descriptor

    VkDescriptorImageInfo renderImageDescriptor{};
    renderImageDescriptor.imageView = renderImage.view;
    renderImageDescriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    
    VkWriteDescriptorSet renderImageWrite{};
    renderImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    renderImageWrite.dstSet = descriptorSet;
    renderImageWrite.descriptorCount = 1;
    renderImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    renderImageWrite.pImageInfo = &renderImageDescriptor;
    renderImageWrite.dstBinding = 1;

    // ubo descriptor

    VkDescriptorBufferInfo uboDescriptor{};
    uboDescriptor.buffer = ubo.buffer;
    uboDescriptor.offset = 0;
        
    VkWriteDescriptorSet uniformBufferWrite{};
    uniformBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    uniformBufferWrite.dstSet = descriptorSet;
    uniformBufferWrite.descriptorCount = 1;
    uniformBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformBufferWrite.pBufferInfo = &uboDescriptor;
    uniformBufferWrite.dstBinding = 2;
}

void RendererRTX::buildCommandBuffers() {
    // allocate command buffers
    commandBuffersDraw.resize(swapchain.numImages);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandBufferCount = commandBuffersDraw.size();
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    vkAllocateCommandBuffers(device, &allocInfo, commandBuffersDraw.data());

    // begin info
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    // Calculate shader binding offsets, which is pretty straight forward in our example 
    VkDeviceSize bindingOffsetRayGenShader = rayTracingProperties.shaderGroupHandleSize * INDEX_RAYGEN;
    VkDeviceSize bindingOffsetMissShader = rayTracingProperties.shaderGroupHandleSize * INDEX_MISS;
    VkDeviceSize bindingOffsetHitShader = rayTracingProperties.shaderGroupHandleSize * INDEX_CLOSEST_HIT;
    VkDeviceSize bindingStride = rayTracingProperties.shaderGroupHandleSize;

    for (int i = 0; i < commandBuffersDraw.size(); i++) {
        VkCommandBuffer& commandBuffer = commandBuffersDraw[i];
        VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &beginInfo), "failed to begin command buffer");

        // ray tracing dispath

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, pipelineLayout, 0, 1, &descriptorSet, 0, 0);

        vkCmdTraceRaysNV(commandBuffer,
            shaderBindingTable.buffer, bindingOffsetRayGenShader,
            shaderBindingTable.buffer, bindingOffsetMissShader, bindingStride,
            shaderBindingTable.buffer, bindingOffsetHitShader, bindingStride,
            VK_NULL_HANDLE, 0, 0,
            width, height, 1);

        // copy ray tracing output to swapchain image

        recordImageLayoutTransition(
            commandBuffer,
            swapchain.images[i],
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresourceRange);

        recordImageLayoutTransition(
            commandBuffer,
            renderImage.image,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            subresourceRange);

        VkImageCopy copyRegion{};
        copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copyRegion.srcOffset = { 0, 0, 0 };
        copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copyRegion.dstOffset = { 0, 0, 0 };
        copyRegion.extent = { width, height, 1 };
        vkCmdCopyImage(commandBuffer, renderImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchain.images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        recordImageLayoutTransition(
            commandBuffer,
            swapchain.images[i],
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            subresourceRange);

        recordImageLayoutTransition(
            commandBuffer,
            renderImage.image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL,
            subresourceRange);
            
        VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer), "failed to end rendering command buffer {}", i);
    }
}

// LOOP

void RendererRTX::DrawFrame(bool framebufferResized) {
    if (!_initialized) {
        AID_WARN("calling DrawFrame() when the renderer is not initialized! returning...");
        return;
    }

    vkWaitForFences(device, 1, &inFlightFences[_currentFrame], VK_TRUE, DEFAULT_FENCE_TIMEOUT);

    uint32_t imageIndex;
    VkResult resultAcquire = vkAcquireNextImageKHR(device, swapchain.swapchain, UINT64_MAX, imageAvailableSemaphores[_currentFrame], VK_NULL_HANDLE, &imageIndex);

    // TODO framebufferResized? debug and check result values
    if (resultAcquire == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (resultAcquire != VK_SUCCESS && resultAcquire != VK_SUBOPTIMAL_KHR) {
        AID_ERROR("failed to acquire swap chain image!");
    }

    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, DEFAULT_FENCE_TIMEOUT);
    }
    imagesInFlight[imageIndex] = inFlightFences[_currentFrame];

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[_currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[_currentFrame] };

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffersDraw[imageIndex];
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VK_CHECK_RESULT(vkQueueSubmit(queues.graphics, 1, &submitInfo, inFlightFences[_currentFrame]), "failed to submit render queue {}", imageIndex);

    VkSwapchainKHR swapchains[] = { swapchain.swapchain };
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    VkResult resultPresent = vkQueuePresentKHR(queues.present, &presentInfo);

    if (resultPresent == VK_ERROR_OUT_OF_DATE_KHR || resultPresent == VK_SUBOPTIMAL_KHR || framebufferResized) {
        recreateSwapChain();
    } else if (resultPresent != VK_SUCCESS) {
        AID_ERROR("failed to present swap chain image!");
    }

    _currentFrame = (_currentFrame + 1) % AIDANIC_CONFIG::maxFramesInFlight;
}

// CLEAN UP

void RendererRTX::CleanUp() {
    vkDestroyPipeline(device, pipeline, VK_ALLOCATOR);
    vkDestroyPipelineLayout(device, pipelineLayout, VK_ALLOCATOR);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, VK_ALLOCATOR);

    vkFreeMemory(device, bottomLevelAS.memory, VK_ALLOCATOR);
    vkFreeMemory(device, topLevelAS.memory, VK_ALLOCATOR);
    vkFreeMemory(device, renderImage.memory, VK_ALLOCATOR);
    vkFreeMemory(device, shaderBindingTable.memory, VK_ALLOCATOR);
    vkFreeMemory(device, ubo.memory, VK_ALLOCATOR);

    vkDestroyImageView(device, renderImage.view, VK_ALLOCATOR);
    vkDestroyImage(device, renderImage.image, VK_ALLOCATOR);

    vkDestroyBuffer(device, shaderBindingTable.buffer, VK_ALLOCATOR);
    vkDestroyBuffer(device, ubo.buffer, VK_ALLOCATOR);

    vkDestroyAccelerationStructureNV(device, bottomLevelAS.accelerationStructure, nullptr);
    vkDestroyAccelerationStructureNV(device, topLevelAS.accelerationStructure, nullptr);

    cleanUpBase();
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

