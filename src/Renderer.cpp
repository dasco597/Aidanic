#include "Renderer.h"
#include "Aidanic.h"
#include "ImGuiVk.h"
#include "tools/config.h"

#include <array>
#include <stdexcept>
#include <set>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// TODO replace nullptr with VK_ALLOCATOR where relevant

// shader files (relative to assets folder, _CONFIG::getAssetsPath())
#define SHADER_SRC_RAYGEN "spirv/raygen.rgen.spv"
#define SHADER_SRC_INTERSECTION "spirv/intersect.rint.spv"
#define SHADER_SRC_CLOSEST_HIT "spirv/closesthit.rchit.spv"
#define SHADER_SRC_MISS "spirv/miss.rmiss.spv"

// shader indices
enum {
    INDEX_RAYGEN,
    INDEX_INTERSECTION,
    INDEX_CLOSEST_HIT,
    INDEX_MISS,
    SHADER_COUNT
};

// INITIALIZATION

void Renderer::init(Aidanic* app, std::vector<const char*>& requiredExtensions, std::vector<Models::Sphere>& spheres, glm::mat4 viewInverse, glm::mat4 projInverse, glm::vec3 cameraPos) {
    AID_INFO("Initializing vulkan renderer...");
    this->aidanicApp = app;

    createInstance(requiredExtensions);
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();

    createSwapChain();
    createCommandPool();
    createSyncObjects();

    uniformData.viewInverse = viewInverse;
    uniformData.projInverse = projInverse;
    uniformData.cameraPos = glm::vec4(cameraPos, 1.f);

    createRenderImage();
    createScene(spheres);

    createRayTracingPipeline();
    createShaderBindingTable();
    createDescriptorSets();
    createCommandBuffersRender();
    createCommandBuffersImageCopy();
}

void Renderer::createInstance(std::vector<const char*>& requiredExtensions) {
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        AID_ERROR("validation layers requested, but not available!");
    }

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Aidanic";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "Aidanic Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    std::vector<const char*> extensions = getRequiredExtensions();
    extensions.insert(extensions.end(), requiredExtensions.begin(), requiredExtensions.end());
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        debugCreateInfo = {};
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;

        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    VK_CHECK_RESULT(vkCreateInstance(&createInfo, VK_ALLOCATOR, &instance), "failed to create instance!");
}

void Renderer::setupDebugMessenger() {
    if (!enableValidationLayers) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;

    VK_CHECK_RESULT(createDebugUtilsMessengerEXT(instance, &createInfo, VK_ALLOCATOR, &debugMessenger), "failed to set up debug messenger!");
}

void Renderer::createSurface() {
    VK_CHECK_RESULT(aidanicApp->createVkSurface(instance, VK_ALLOCATOR, &surface), "failed to create window surface!");
}

void Renderer::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        AID_ERROR("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            physicalDevice = device;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        std::string extensionWarning = "vulkan extensions required by Aidanic: ";
        for (const char* extension : deviceExtensions) extensionWarning += std::string(extension) + ", ";
        AID_WARN(extensionWarning);
        AID_ERROR("failed to find a suitable GPU!");
    }

    // get physical device properties and limits
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

    // Query the ray tracing properties of the current implementation
    rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;
    VkPhysicalDeviceProperties2 deviceProps2{};
    deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProps2.pNext = &rayTracingProperties;
    vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProps2);
}

void Renderer::createLogicalDevice() {
    Vk::QueueFamilyIndices queueIndices = Vk::findQueueFamilies(physicalDevice, surface);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { queueIndices.graphicsFamily.value(), queueIndices.computeFamily.value(), queueIndices.presentFamily.value() };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures = {};

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    VK_CHECK_RESULT(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device), "failed to create logical device!");

    vkGetDeviceQueue(device, queueIndices.graphicsFamily.value(), 0, &queues.graphics);
    vkGetDeviceQueue(device, queueIndices.computeFamily.value(), 0, &queues.compute);
    vkGetDeviceQueue(device, queueIndices.presentFamily.value(), 0, &queues.present);

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
}

void Renderer::createSwapChain() {
    Vk::SwapChainSupportDetails swapChainSupport = Vk::querySwapChainSupport(physicalDevice, surface);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    Vk::QueueFamilyIndices queueIndices = Vk::findQueueFamilies(physicalDevice, surface);
    uint32_t queueFamilyIndices[] = { queueIndices.graphicsFamily.value(), queueIndices.presentFamily.value() };

    if (queueIndices.graphicsFamily != queueIndices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    VK_CHECK_RESULT(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain.swapchain), "failed to create swap chain!");

    vkGetSwapchainImagesKHR(device, swapchain.swapchain, &imageCount, nullptr);
    swapchain.images.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain.swapchain, &imageCount, swapchain.images.data());
    swapchain.numImages = imageCount;

    swapchain.format = surfaceFormat.format;
    swapchain.extent = extent;
}

void Renderer::createCommandPool() {
    Vk::QueueFamilyIndices queueFamilyIndices = Vk::findQueueFamilies(physicalDevice, surface);

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    VK_CHECK_RESULT(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool), "failed to create graphics command pool!");
}

void Renderer::createImageViews() {
    swapchain.imageViews.resize(swapchain.numImages);

    for (size_t i = 0; i < swapchain.numImages; i++) {
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapchain.images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapchain.format;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VK_CHECK_RESULT(vkCreateImageView(device, &createInfo, nullptr, &swapchain.imageViews[i]), "failed to create image views!");
    }
}

void Renderer::createSyncObjects() {
    semaphoresImageAvailable.resize(_MAX_FRAMES_IN_FLIGHT);
    semaphoresRenderFinished.resize(_MAX_FRAMES_IN_FLIGHT);
    semaphoresImGuiFinished.resize(_MAX_FRAMES_IN_FLIGHT);
    semaphoresImageCopyFinished.resize(_MAX_FRAMES_IN_FLIGHT);

    inFlightFences.resize(_MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(swapchain.numImages, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < _MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphoresImageAvailable[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphoresRenderFinished[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphoresImGuiFinished[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphoresImageCopyFinished[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

void Renderer::createScene(std::vector<Models::Sphere>& spheres) {
    // create sphere AABBs

    sphereAABBs.resize(spheres.size());
    for (int s = 0; s < spheres.size(); s++) sphereAABBs[s].init(spheres[s]);

    // create buffers

    if (sizeof(Vk::AABB) % 8 != 0) {
        AID_ERROR("Vk::AABB size not a multiple of 8bytes -> add padding to struct or aabb buffer");
    }
    createBuffer(bufferSpheres, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sizeof(Vk::AABB) * sphereAABBs.size());
    uploadBufferDeviceLocal(bufferSpheres, sphereAABBs.data(), sizeof(Vk::AABB) * sphereAABBs.size());

    bufferUBO.dynamicStride = getUBOOffsetAligned(sizeof(UniformData));
    createBuffer(bufferUBO, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, static_cast<VkDeviceSize>(swapchain.numImages) * bufferUBO.dynamicStride);
    for (int i = 0; i < swapchain.numImages; i++) {
        uploadBufferHostVisible(bufferUBO, &uniformData, sizeof(UniformData), static_cast<VkDeviceSize>(i) * bufferUBO.dynamicStride);
    }

    // create a bottom-level acceleration structure containing the scene geometry

    VkGeometryAABBNV geometryAabbSphere = {};
    geometryAabbSphere.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
    geometryAabbSphere.aabbData = bufferSpheres.buffer;
    geometryAabbSphere.numAABBs = sphereAABBs.size();
    geometryAabbSphere.stride = sizeof(Vk::AABB);
    geometryAabbSphere.offset = 0;

    VkGeometryNV geometrySphere{};
    geometrySphere.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
    geometrySphere.flags = VK_GEOMETRY_OPAQUE_BIT_NV;
    geometrySphere.geometryType = VK_GEOMETRY_TYPE_AABBS_NV;
    geometrySphere.geometry.aabbs = geometryAabbSphere;

    geometrySphere.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
    geometrySphere.geometry.triangles.vertexData = VK_NULL_HANDLE;
    geometrySphere.geometry.triangles.vertexCount = 0;
    geometrySphere.geometry.triangles.vertexOffset = 0;
    geometrySphere.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometrySphere.geometry.triangles.indexData = VK_NULL_HANDLE;
    geometrySphere.geometry.triangles.indexCount = 0;
    geometrySphere.geometry.triangles.indexOffset = 0;
    geometrySphere.geometry.triangles.indexType = VK_INDEX_TYPE_NONE_NV;
    geometrySphere.geometry.triangles.transformData = VK_NULL_HANDLE;
    geometrySphere.geometry.triangles.transformOffset = 0;

    createBottomLevelAccelerationStructure(blasSphere, &geometrySphere, 1);

    // create the top-level acceleration structure containing BLAS instance information

    glm::mat3x4 transform = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };

    Vk::BLASInstance geometryInstance{};
    geometryInstance.transform = transform;
    geometryInstance.instanceId = 0;
    geometryInstance.mask = 0xff;
    geometryInstance.instanceOffset = 0;
    geometryInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;
    geometryInstance.accelerationStructureHandle = blasSphere.handle;

    Vk::Buffer instanceBuffer;
    createBuffer(instanceBuffer, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, sizeof(Vk::BLASInstance));
    uploadBufferHostVisible(instanceBuffer, &geometryInstance, sizeof(Vk::BLASInstance));

    createTopLevelAccelerationStructure();

    // acceleration structure building requires some scratch space to store temporary information

    VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo{};
    memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
    memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;

    VkMemoryRequirements2 memReqBottomLevelAS;
    memoryRequirementsInfo.accelerationStructure = blasSphere.accelerationStructure;
    vkGetAccelerationStructureMemoryRequirementsNV(device, &memoryRequirementsInfo, &memReqBottomLevelAS);

    VkMemoryRequirements2 memReqTopLevelAS;
    memoryRequirementsInfo.accelerationStructure = tlas.accelerationStructure;
    vkGetAccelerationStructureMemoryRequirementsNV(device, &memoryRequirementsInfo, &memReqTopLevelAS);

    const VkDeviceSize scratchBufferSize = std::max(memReqBottomLevelAS.memoryRequirements.size, memReqTopLevelAS.memoryRequirements.size);

    Vk::Buffer scratchBuffer;
    createBuffer(scratchBuffer, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBufferSize);

    VkCommandBuffer cmdBuffer = Vk::beginSingleTimeCommands(device, commandPool);

    // build bottom-level acceleration structure

    VkAccelerationStructureInfoNV buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
    buildInfo.instanceCount = 0;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometrySphere;

    vkCmdBuildAccelerationStructureNV(
        cmdBuffer,
        &buildInfo,
        VK_NULL_HANDLE,
        0,
        VK_FALSE,
        blasSphere.accelerationStructure,
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
        tlas.accelerationStructure,
        VK_NULL_HANDLE,
        scratchBuffer.buffer,
        0);

    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 0, 1, &memoryBarrier, 0, 0, 0, 0);

    Vk::endSingleTimeCommands(device, cmdBuffer, queues.compute, commandPool);

    destroyBuffer(instanceBuffer);
    destroyBuffer(scratchBuffer);
}

void Renderer::createBottomLevelAccelerationStructure(Vk::AccelerationStructure& blas, const VkGeometryNV* geometries, uint32_t geometryCount) {
    VkAccelerationStructureInfoNV accelerationStructureInfo{};
    accelerationStructureInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
    accelerationStructureInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
    accelerationStructureInfo.instanceCount = 0;
    accelerationStructureInfo.geometryCount = geometryCount;
    accelerationStructureInfo.pGeometries = geometries;

    VkAccelerationStructureCreateInfoNV accelerationStructureCI{};
    accelerationStructureCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
    accelerationStructureCI.info = accelerationStructureInfo;
    VK_CHECK_RESULT(vkCreateAccelerationStructureNV(device, &accelerationStructureCI, VK_ALLOCATOR, &blas.accelerationStructure), "failed to create bottom level acceleration structure");

    VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo{};
    memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
    memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;
    memoryRequirementsInfo.accelerationStructure = blas.accelerationStructure;

    VkMemoryRequirements2 memoryRequirements{};
    vkGetAccelerationStructureMemoryRequirementsNV(device, &memoryRequirementsInfo, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = Vk::findMemoryType(physicalDevice, memoryRequirements.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, VK_ALLOCATOR, &blas.memory), "failed to allocate bottom level acceleration structure memory");

    VkBindAccelerationStructureMemoryInfoNV accelerationStructureMemoryInfo{};
    accelerationStructureMemoryInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
    accelerationStructureMemoryInfo.accelerationStructure = blas.accelerationStructure;
    accelerationStructureMemoryInfo.memory = blas.memory;
    VK_CHECK_RESULT(vkBindAccelerationStructureMemoryNV(device, 1, &accelerationStructureMemoryInfo), "failed to bind acceleration structure memory");

    VK_CHECK_RESULT(vkGetAccelerationStructureHandleNV(device, blas.accelerationStructure, sizeof(uint64_t), &blas.handle), "failed to get bottom level acceleration structure handle");
}

void Renderer::createTopLevelAccelerationStructure() {
    VkAccelerationStructureInfoNV accelerationStructureInfo{};
    accelerationStructureInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
    accelerationStructureInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
    accelerationStructureInfo.instanceCount = 1;
    accelerationStructureInfo.geometryCount = 0;

    VkAccelerationStructureCreateInfoNV accelerationStructureCI{};
    accelerationStructureCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
    accelerationStructureCI.info = accelerationStructureInfo;
    VK_CHECK_RESULT(vkCreateAccelerationStructureNV(device, &accelerationStructureCI, VK_ALLOCATOR, &tlas.accelerationStructure), "failed to create top level acceleration stucture");

    VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo{};
    memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
    memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;
    memoryRequirementsInfo.accelerationStructure = tlas.accelerationStructure;

    VkMemoryRequirements2 memoryRequirements{};
    vkGetAccelerationStructureMemoryRequirementsNV(device, &memoryRequirementsInfo, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = Vk::findMemoryType(physicalDevice, memoryRequirements.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, VK_ALLOCATOR, &tlas.memory), "failed to allocate top level acceleration structure memory");

    VkBindAccelerationStructureMemoryInfoNV accelerationStructureMemoryInfo{};
    accelerationStructureMemoryInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
    accelerationStructureMemoryInfo.accelerationStructure = tlas.accelerationStructure;
    accelerationStructureMemoryInfo.memory = tlas.memory;
    VK_CHECK_RESULT(vkBindAccelerationStructureMemoryNV(device, 1, &accelerationStructureMemoryInfo), "failed to bind acceleration structure memory");

    VK_CHECK_RESULT(vkGetAccelerationStructureHandleNV(device, tlas.accelerationStructure, sizeof(uint64_t), &tlas.handle), "failed to get top level acceleration structure handle");
}

void Renderer::createRenderImage() {
    renderImage.extent = swapchain.extent;
    renderImage.format = swapchain.format;

    VkImageCreateInfo imageCI = {};
    imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = renderImage.format;
    imageCI.extent.width = renderImage.extent.width;
    imageCI.extent.height = renderImage.extent.height;
    imageCI.extent.depth = 1;
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CHECK_RESULT(vkCreateImage(device, &imageCI, VK_ALLOCATOR, &renderImage.image), "failed to create ray tracing storage image");

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, renderImage.image, &memReqs);
    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memReqs.size;
    memoryAllocateInfo.memoryTypeIndex = Vk::findMemoryType(physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &renderImage.memory), "failed to allocate render image memory");
    VK_CHECK_RESULT(vkBindImageMemory(device, renderImage.image, renderImage.memory, 0), "failed to bind image memory");

    VkImageViewCreateInfo colorImageView{};
    colorImageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    colorImageView.format = renderImage.format;
    colorImageView.subresourceRange = {};
    colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorImageView.subresourceRange.baseMipLevel = 0;
    colorImageView.subresourceRange.levelCount = 1;
    colorImageView.subresourceRange.baseArrayLayer = 0;
    colorImageView.subresourceRange.layerCount = 1;
    colorImageView.image = renderImage.image;
    VK_CHECK_RESULT(vkCreateImageView(device, &colorImageView, nullptr, &renderImage.view), "failed to create render image view");

    VkCommandBuffer cmdBuffer = Vk::beginSingleTimeCommands(device, commandPool);
    recordImageLayoutTransition(cmdBuffer, renderImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
    Vk::endSingleTimeCommands(device, cmdBuffer, queues.graphics, commandPool);
}

void Renderer::createRayTracingPipeline() {
    VkDescriptorSetLayoutBinding layoutBindingAccelerationStructure{};
    layoutBindingAccelerationStructure.binding = 0;
    layoutBindingAccelerationStructure.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
    layoutBindingAccelerationStructure.descriptorCount = 1;
    layoutBindingAccelerationStructure.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;

    VkDescriptorSetLayoutBinding layoutBindingRenderImage{};
    layoutBindingRenderImage.binding = 1;
    layoutBindingRenderImage.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    layoutBindingRenderImage.descriptorCount = 1;
    layoutBindingRenderImage.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;

    VkDescriptorSetLayoutBinding layoutBindingUniformBuffer{};
    layoutBindingUniformBuffer.binding = 2;
    layoutBindingUniformBuffer.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    layoutBindingUniformBuffer.descriptorCount = 1;
    layoutBindingUniformBuffer.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;

    VkDescriptorSetLayoutBinding layoutBindingSpheresBuffer{};
    layoutBindingSpheresBuffer.binding = 3;
    layoutBindingSpheresBuffer.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindingSpheresBuffer.descriptorCount = 1;
    layoutBindingSpheresBuffer.stageFlags = VK_SHADER_STAGE_INTERSECTION_BIT_NV;

    std::vector<VkDescriptorSetLayoutBinding> bindings({
        layoutBindingAccelerationStructure,
        layoutBindingRenderImage,
        layoutBindingUniformBuffer,
        layoutBindingSpheresBuffer });

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI{};
    descriptorSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCI.bindingCount = static_cast<uint32_t>(bindings.size());
    descriptorSetLayoutCI.pBindings = bindings.data();
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, VK_ALLOCATOR, &descriptorSetLayout), "failed to create descriptor set layout");

    VkPipelineLayoutCreateInfo pipelineLayoutCI{};
    pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCI.setLayoutCount = 1;
    pipelineLayoutCI.pSetLayouts = &descriptorSetLayout;
    vkCreatePipelineLayout(device, &pipelineLayoutCI, VK_ALLOCATOR, &pipelineLayout);

    std::array<VkPipelineShaderStageCreateInfo, SHADER_COUNT> shaderStages;
    std::array<VkShaderModule, SHADER_COUNT> shaderModules;
    shaderStages[INDEX_RAYGEN] = Vk::loadShader(device, std::string(_CONFIG::getAssetsPath()) + std::string(SHADER_SRC_RAYGEN), VK_SHADER_STAGE_RAYGEN_BIT_NV, shaderModules[INDEX_RAYGEN]);
    shaderStages[INDEX_INTERSECTION] = Vk::loadShader(device, std::string(_CONFIG::getAssetsPath()) + std::string(SHADER_SRC_INTERSECTION), VK_SHADER_STAGE_INTERSECTION_BIT_NV, shaderModules[INDEX_INTERSECTION]);
    //shaderStages[INDEX_ANY_HIT] = Vk::loadShader(device, std::string(_CONFIG::getAssetsPath()) + std::string(SHADER_SRC_ANY_HIT), VK_SHADER_STAGE_ANY_HIT_BIT_NV, shaderModules[INDEX_ANY_HIT]);
    shaderStages[INDEX_CLOSEST_HIT] = Vk::loadShader(device, std::string(_CONFIG::getAssetsPath()) + std::string(SHADER_SRC_CLOSEST_HIT), VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, shaderModules[INDEX_CLOSEST_HIT]);
    shaderStages[INDEX_MISS] = Vk::loadShader(device, std::string(_CONFIG::getAssetsPath()) + std::string(SHADER_SRC_MISS), VK_SHADER_STAGE_MISS_BIT_NV, shaderModules[INDEX_MISS]);

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

    shaderGroups[INDEX_INTERSECTION].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_NV;
    shaderGroups[INDEX_INTERSECTION].intersectionShader = INDEX_INTERSECTION;

    //shaderGroups[INDEX_ANY_HIT].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_NV;
    //shaderGroups[INDEX_ANY_HIT].anyHitShader = INDEX_ANY_HIT;
    //shaderGroups[INDEX_ANY_HIT].intersectionShader = INDEX_INTERSECTION;

    shaderGroups[INDEX_CLOSEST_HIT].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_NV;
    shaderGroups[INDEX_CLOSEST_HIT].closestHitShader = INDEX_CLOSEST_HIT;
    shaderGroups[INDEX_CLOSEST_HIT].intersectionShader = INDEX_INTERSECTION;

    shaderGroups[INDEX_MISS].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
    shaderGroups[INDEX_MISS].generalShader = INDEX_MISS;

    VkRayTracingPipelineCreateInfoNV rayPipelineCI{};
    rayPipelineCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV;
    rayPipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    rayPipelineCI.pStages = shaderStages.data();
    rayPipelineCI.groupCount = static_cast<uint32_t>(shaderGroups.size());
    rayPipelineCI.pGroups = shaderGroups.data();
    rayPipelineCI.maxRecursionDepth = 1;
    rayPipelineCI.layout = pipelineLayout;
    VK_CHECK_RESULT(vkCreateRayTracingPipelinesNV(device, VK_NULL_HANDLE, 1, &rayPipelineCI, VK_ALLOCATOR, &pipeline), "failed to create ray tracing pipeline");

    for (VkShaderModule shaderModule : shaderModules)
        vkDestroyShaderModule(device, shaderModule, VK_ALLOCATOR);
}

void Renderer::createShaderBindingTable() {
    uint32_t sbtSize = rayTracingProperties.shaderGroupHandleSize * SHADER_COUNT;
    uint8_t* shaderGroupHandleStorage = new uint8_t[sbtSize];
    VK_CHECK_RESULT(vkGetRayTracingShaderGroupHandlesNV(device, pipeline, 0, SHADER_COUNT, sbtSize, shaderGroupHandleStorage), "failed to get ray tracing shader group handles");

    createBuffer(shaderBindingTable, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, sbtSize);
    uploadBufferHostVisible(shaderBindingTable, shaderGroupHandleStorage, sbtSize);
}

void Renderer::createDescriptorSets() {
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 }
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
    descriptorAccelerationStructureInfo.pAccelerationStructures = &tlas.accelerationStructure;

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
    uboDescriptor.buffer = bufferUBO.buffer;
    uboDescriptor.offset = 0;
    uboDescriptor.range = bufferUBO.dynamicStride;

    VkWriteDescriptorSet uniformBufferWrite{};
    uniformBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    uniformBufferWrite.dstSet = descriptorSet;
    uniformBufferWrite.descriptorCount = 1;
    uniformBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    uniformBufferWrite.pBufferInfo = &uboDescriptor;
    uniformBufferWrite.dstBinding = 2;

    // sphere aabbs ssbo

    VkDescriptorBufferInfo spheresDescriptor{};
    spheresDescriptor.buffer = bufferSpheres.buffer;
    spheresDescriptor.offset = 0;
    spheresDescriptor.range = bufferSpheres.size;

    VkWriteDescriptorSet spheresWrite{};
    spheresWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    spheresWrite.dstSet = descriptorSet;
    spheresWrite.descriptorCount = 1;
    spheresWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    spheresWrite.pBufferInfo = &spheresDescriptor;
    spheresWrite.dstBinding = 3;

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        accelerationStructureWrite,
        renderImageWrite,
        uniformBufferWrite,
        spheresWrite
    };
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
}

void Renderer::createCommandBuffersRender() {
    commandBuffersRender.resize(swapchain.numImages);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandBufferCount = commandBuffersRender.size();
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    vkAllocateCommandBuffers(device, &allocInfo, commandBuffersRender.data());

    // begin info
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    // Calculate shader binding offsets, which is pretty straight forward in our example 
    VkDeviceSize bindingOffsetRayGenShader = rayTracingProperties.shaderGroupHandleSize * INDEX_RAYGEN;
    VkDeviceSize bindingOffsetHitShader = rayTracingProperties.shaderGroupHandleSize * INDEX_CLOSEST_HIT;
    VkDeviceSize bindingOffsetMissShader = rayTracingProperties.shaderGroupHandleSize * INDEX_MISS;
    VkDeviceSize bindingStride = rayTracingProperties.shaderGroupHandleSize;

    for (int i = 0; i < commandBuffersRender.size(); i++) {
        VkCommandBuffer& commandBuffer = commandBuffersRender[i];
        VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &beginInfo), "failed to begin command buffer");

        // ray tracing dispath

        uint32_t uboDynamicOffset = i * bufferUBO.dynamicStride;

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, pipelineLayout, 0, 1, &descriptorSet, 1, &uboDynamicOffset);

        vkCmdTraceRaysNV(commandBuffer,
            shaderBindingTable.buffer, bindingOffsetRayGenShader,
            shaderBindingTable.buffer, bindingOffsetMissShader, bindingStride,
            shaderBindingTable.buffer, bindingOffsetHitShader, bindingStride,
            VK_NULL_HANDLE, 0, 0,
            swapchain.extent.width, swapchain.extent.height, 1);

        VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer), "failed to end rendering command buffer {}", i);
    }
}

void Renderer::createCommandBuffersImageCopy() {
    commandBuffersImageCopy.resize(swapchain.numImages);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandBufferCount = commandBuffersImageCopy.size();
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    vkAllocateCommandBuffers(device, &allocInfo, commandBuffersImageCopy.data());

    // begin info
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    for (int i = 0; i < commandBuffersImageCopy.size(); i++) {
        VkCommandBuffer& commandBuffer = commandBuffersImageCopy[i];
        VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &beginInfo), "failed to begin command buffer");

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
        copyRegion.extent = { swapchain.extent.width, swapchain.extent.height, 1 };
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

// MAIN LOOP

void Renderer::drawFrame(bool framebufferResized, glm::mat4 viewInverse, glm::mat4 projInverse, glm::vec3 cameraPos, ImGuiVk* imGuiRenderer, bool renderImGui) {
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, DEFAULT_FENCE_TIMEOUT);

    uint32_t imageIndex;
    VkResult resultAcquire = vkAcquireNextImageKHR(device, swapchain.swapchain, UINT64_MAX, semaphoresImageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);

    // TODO framebufferResized? debug and check result values
    if (resultAcquire == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        if (imGuiRenderer) imGuiRenderer->recreateFramebuffer();
        return;
    } else if (resultAcquire != VK_SUCCESS && resultAcquire != VK_SUBOPTIMAL_KHR) {
        AID_ERROR("failed to acquire swap chain image!");
    }

    updateUniformBuffer(viewInverse, projInverse, glm::vec4(cameraPos, 1.f), imageIndex);

    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, DEFAULT_FENCE_TIMEOUT);
    }
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];

    // ray tracing dispatch
    {
        VkSemaphore signalSemaphores[] = { semaphoresRenderFinished[currentFrame] };

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffersRender[imageIndex];
        submitInfo.waitSemaphoreCount = 0;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VK_CHECK_RESULT(vkQueueSubmit(queues.graphics, 1, &submitInfo, VK_NULL_HANDLE), "failed to submit render queue {}", imageIndex);
    }

    // render imGui
    if (imGuiRenderer && renderImGui) imGuiRenderer->recordRenderCommands(imageIndex);
    if (imGuiRenderer) renderImGui &= imGuiRenderer->shouldRender(imageIndex);
    if (renderImGui) {

        VkSemaphore waitSemaphores[] = { semaphoresRenderFinished[currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSemaphore signalSemaphores[] = { semaphoresImGuiFinished[currentFrame] };

        VkCommandBuffer commandBuffers[] = { imGuiRenderer->getCommandBuffer(imageIndex) };

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = commandBuffers;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VK_CHECK_RESULT(vkQueueSubmit(queues.graphics, 1, &submitInfo, VK_NULL_HANDLE), "failed to submit render queue {}", imageIndex);
    }

    // copy render image to swapchain image
    {
        VkSemaphore signalSemaphores[] = { semaphoresImageCopyFinished[currentFrame] };
        VkSemaphore waitSemaphores[2];
        waitSemaphores[0] = semaphoresImageAvailable[currentFrame];
        if (renderImGui) waitSemaphores[1] = semaphoresImGuiFinished[currentFrame];
        else waitSemaphores[1] = semaphoresRenderFinished[currentFrame];
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT };

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffersImageCopy[imageIndex];
        submitInfo.waitSemaphoreCount = 2;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkResetFences(device, 1, &inFlightFences[currentFrame]);
        VK_CHECK_RESULT(vkQueueSubmit(queues.graphics, 1, &submitInfo, inFlightFences[currentFrame]), "failed to submit render queue {}", imageIndex);
    }

    // present
    {
        VkSemaphore waitSemaphores[] = { semaphoresImageCopyFinished[currentFrame] };

        VkSwapchainKHR swapchains[] = { swapchain.swapchain };
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = waitSemaphores;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;

        VkResult resultPresent = vkQueuePresentKHR(queues.present, &presentInfo);

        if (resultPresent == VK_ERROR_OUT_OF_DATE_KHR || resultPresent == VK_SUBOPTIMAL_KHR || framebufferResized) {
            recreateSwapChain();
            if (imGuiRenderer) imGuiRenderer->recreateFramebuffer();
        } else if (resultPresent != VK_SUCCESS) {
            AID_ERROR("failed to present swap chain image!");
        }
    }

    currentFrame = (currentFrame + 1) % _MAX_FRAMES_IN_FLIGHT;
}

void Renderer::updateUniformBuffer(glm::mat4 viewInverse, glm::mat4 projInverse, glm::vec4 cameraPos, uint32_t swapchainIndex) {
    uniformData.viewInverse = viewInverse;
    uniformData.projInverse = projInverse;
    uniformData.cameraPos = cameraPos;

    uploadBufferHostVisible(bufferUBO, &uniformData, sizeof(UniformData), static_cast<VkDeviceSize>(swapchainIndex) * bufferUBO.dynamicStride);
}

void Renderer::recreateSwapChain() {
    vkDeviceWaitIdle(device);

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createRenderImage();
    createDescriptorSets();
    createCommandBuffersRender();
    createCommandBuffersImageCopy();
}

VKAPI_ATTR VkBool32 VKAPI_CALL Renderer::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {

    std::string severityString;
    switch (messageSeverity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: severityString = "V"; break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:    severityString = "I"; break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: severityString = "W"; break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:   severityString = "E"; break;
    default: severityString = "";
    }

    if (messageSeverity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)) {
        AID_INFO(std::string("- Vulkan validation {} - ") + std::string(pCallbackData->pMessage), severityString);
    } else if (messageSeverity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) {
        AID_WARN(std::string("- Vulkan validation {} - ") + std::string(pCallbackData->pMessage), severityString);
    }

    return VK_FALSE;
}

// CLEANING UP

void Renderer::cleanUp() {
    vkDeviceWaitIdle(device);
    AID_INFO("Cleaning up Vulkan renderer...");

    vkDestroyPipeline(device, pipeline, VK_ALLOCATOR);
    vkDestroyPipelineLayout(device, pipelineLayout, VK_ALLOCATOR);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, VK_ALLOCATOR);

    vkFreeMemory(device, blasSphere.memory, VK_ALLOCATOR);
    vkFreeMemory(device, tlas.memory, VK_ALLOCATOR);

    vkDestroyAccelerationStructureNV(device, blasSphere.accelerationStructure, nullptr);
    vkDestroyAccelerationStructureNV(device, tlas.accelerationStructure, nullptr);

    destroyBuffer(bufferSpheres);
    destroyBuffer(bufferUBO);
    destroyBuffer(shaderBindingTable);

    cleanupSwapChain();

    for (size_t i = 0; i < _MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, semaphoresImageAvailable[i], VK_ALLOCATOR);
        vkDestroySemaphore(device, semaphoresRenderFinished[i], VK_ALLOCATOR);
        vkDestroySemaphore(device, semaphoresImGuiFinished[i], VK_ALLOCATOR);
        vkDestroySemaphore(device, semaphoresImageCopyFinished[i], VK_ALLOCATOR);
        vkDestroyFence(device, inFlightFences[i], VK_ALLOCATOR);
    }
    vkDestroyCommandPool(device, commandPool, VK_ALLOCATOR);

    vkDestroyDevice(device, VK_ALLOCATOR);

    if (enableValidationLayers)
        destroyDebugUtilsMessengerEXT(instance, debugMessenger, VK_ALLOCATOR);

    vkDestroySurfaceKHR(instance, surface, VK_ALLOCATOR);
    vkDestroyInstance(instance, VK_ALLOCATOR);
}

void Renderer::cleanupSwapChain() {
    for (auto imageView : swapchain.imageViews) vkDestroyImageView(device, imageView, VK_ALLOCATOR);
    vkDestroySwapchainKHR(device, swapchain.swapchain, VK_ALLOCATOR);
    vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffersRender.size()), commandBuffersRender.data());
    vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffersImageCopy.size()), commandBuffersImageCopy.data());
    commandBuffersRender.clear();
    commandBuffersImageCopy.clear();
    vkDestroyDescriptorPool(device, descriptorPool, VK_ALLOCATOR);
    renderImage.destroy(device);
}

// HELPER FUNCTIONS

bool Renderer::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) return false;
    }

    return true;
}

VkResult Renderer::createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void Renderer::destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

void Renderer::testDebugMessenger() {
    VkDebugUtilsMessengerCallbackDataEXT debugInitMessage = {};
    debugInitMessage.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
    debugInitMessage.pMessageIdName = "Initialization";
    debugInitMessage.messageIdNumber = 0;
    debugInitMessage.pMessage = "Debug messenger initialized!";

    auto func = (PFN_vkSubmitDebugUtilsMessageEXT)vkGetInstanceProcAddr(instance, "vkSubmitDebugUtilsMessageEXT");
    func(instance, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &debugInitMessage);
}

bool Renderer::isDeviceSuitable(VkPhysicalDevice device) {
    Vk::QueueFamilyIndices indices = Vk::findQueueFamilies(device, surface);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        Vk::SwapChainSupportDetails swapChainSupport = Vk::querySwapChainSupport(device, surface);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

bool Renderer::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

std::vector<const char*> Renderer::getRequiredExtensions() {
    std::vector<const char*> extensions;

    if (enableValidationLayers)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    for (auto extension : instanceExtensions)
        extensions.push_back(extension);

    return extensions;
}

VkSurfaceFormatKHR Renderer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM) { //availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR Renderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Renderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        std::array<int, 2> windowSize = aidanicApp->getWindowSize();

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(windowSize[0]),
            static_cast<uint32_t>(windowSize[1])
        };

        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}

void Renderer::recordImageLayoutTransition(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
    VkImageSubresourceRange subresourceRange, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask) {
    // Create an image barrier object
    VkImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.oldLayout = oldImageLayout;
    imageMemoryBarrier.newLayout = newImageLayout;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange = subresourceRange;

    // Source layouts (old)
    // Source access mask controls actions that have to be finished on the old layout
    // before it will be transitioned to the new layout
    switch (oldImageLayout)
    {
    case VK_IMAGE_LAYOUT_GENERAL:
        imageMemoryBarrier.dstAccessMask = 0;
        break;

    case VK_IMAGE_LAYOUT_UNDEFINED:
        // Image layout is undefined (or does not matter)
        // Only valid as initial layout
        // No flags required, listed only for completeness
        imageMemoryBarrier.srcAccessMask = 0;
        break;

    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        // Image is preinitialized
        // Only valid as initial layout for linear images, preserves memory contents
        // Make sure host writes have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        // Image is a color attachment
        // Make sure any writes to the color buffer have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        // Image is a depth/stencil attachment
        // Make sure any writes to the depth/stencil buffer have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        // Image is a transfer source 
        // Make sure any reads from the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        // Image is a transfer destination
        // Make sure any writes to the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        // Image is read by a shader
        // Make sure any shader reads from the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;

    default:
        // Other source layouts aren't handled (yet)
        AID_WARN("Renderer::recordSetImageLayout oldImageLayout not supported!");
        break;
    }

    // Target layouts (new)
    // Destination access mask controls the dependency for the new image layout
    switch (newImageLayout)
    {
    case VK_IMAGE_LAYOUT_GENERAL:
        imageMemoryBarrier.dstAccessMask = 0;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        // Image will be used as a transfer destination, make sure any writes to the image have been finished
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        // Image will be used as a transfer source
        // Make sure any reads from the image have been finished
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        // Image will be used as a color attachment
        // Make sure any writes to the color buffer have been finished
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        // Image layout will be used as a depth/stencil attachment
        // Make sure any writes to depth/stencil buffer have been finished
        imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        // Image will be read in a shader (sampler, input attachment)
        // Make sure any writes to the image have been finished
        if (imageMemoryBarrier.srcAccessMask == 0)
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        // idk lol
        imageMemoryBarrier.dstAccessMask = 0;
        break;

    default:
        // Other source layouts aren't handled (yet)
        AID_WARN("Renderer::recordSetImageLayout newImageLayout not supported!");
        break;
    }

    // Put barrier inside setup command buffer
    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask,
        dstStageMask,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier);
}

void Renderer::createBuffer(Vk::Buffer& buffer, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkDeviceSize size) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK_RESULT(vkCreateBuffer(device, &bufferInfo, VK_ALLOCATOR, &buffer.buffer), "failed to create buffer!");

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer.buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = Vk::findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    VK_CHECK_RESULT(vkAllocateMemory(device, &allocInfo, VK_ALLOCATOR, &buffer.memory), "failed to allocate buffer memory!");

    vkBindBufferMemory(device, buffer.buffer, buffer.memory, 0);
    buffer.size = size;
}

void Renderer::uploadBufferDeviceLocal(Vk::Buffer& buffer, void* data, VkDeviceSize size, VkDeviceSize bufferOffset) {
    Vk::Buffer stagingBuffer;
    createBuffer(stagingBuffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, size);
    uploadBufferHostVisible(stagingBuffer, data, size, 0);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = bufferOffset;

    VkCommandBuffer commandBuffer = Vk::beginSingleTimeCommands(device, commandPool);
    vkCmdCopyBuffer(commandBuffer, stagingBuffer.buffer, buffer.buffer, 1, &copyRegion);
    Vk::endSingleTimeCommands(device, commandBuffer, queues.graphics, commandPool);

    destroyBuffer(stagingBuffer);
}

void Renderer::uploadBufferHostVisible(Vk::Buffer& buffer, void* data, VkDeviceSize size, VkDeviceSize bufferOffset) {
    void* dataDst;
    VK_CHECK_RESULT(vkMapMemory(device, buffer.memory, bufferOffset, size, 0, &dataDst), "failed to map buffer memory");
    memcpy(dataDst, data, (size_t)size);
    vkUnmapMemory(device, buffer.memory);
}

void Renderer::destroyBuffer(Vk::Buffer& buffer) {
    vkDestroyBuffer(device, buffer.buffer, VK_ALLOCATOR);
    vkFreeMemory(device, buffer.memory, VK_ALLOCATOR);
}

VkDeviceSize Renderer::getUBOOffsetAligned(VkDeviceSize stride) {
    VkDeviceSize minAlignment = physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
    return minAlignment * static_cast<VkDeviceSize>((stride + minAlignment - 1) / minAlignment); // minAlignment * ceil(stride / minAlignment)
}