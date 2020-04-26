#include "Renderer.h"

#include "Aidanic.h"
#include "IOInterface.h"
#include "ImGuiVk.h"
#include "tools/config.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm.hpp>
#include "ext/matrix_transform.hpp"

#include <string>
#include <stdexcept>
#include <set>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// shader files (relative to assets folder, _CONFIG::getAssetsPath())
#define SHADER_SRC_RAYGEN               "spirv/scene.rgen.spv"
#define SHADER_SRC_MISS_BACKGROUND      "spirv/background.rmiss.spv"
#define SHADER_SRC_MISS_SHADOW          "spirv/shadow.rmiss.spv"
#define SHADER_SRC_CLOSEST_HIT_SCENE    "spirv/scene.rchit.spv"
#define SHADER_SRC_INTERSECTION_ELLIPSOID  "spirv/ellipsoid.rint.spv"

// shader stage indices
enum {
    STAGE_RAYGEN,
    STAGE_MISS_BACKGROUND,
    STAGE_MISS_SHADOW,
    STAGE_CLOSEST_HIT_SCENE,
    STAGE_INTERSECTION_ELLIPSOID,
    STAGE_COUNT
};

// shader group indices
enum {
    GROUP_RAYGEN,
    GROUP_MISS_BACKGROUND,
    GROUP_MISS_SHADOW,
    GROUP_HIT_SCENE,
    GROUP_COUNT
};


namespace Renderer {

#pragma region PRIVATE_VARIABLES

VkInstance instance;
VkDebugUtilsMessengerEXT debugMessenger;
VkSurfaceKHR surface;

VkDevice device;
VkPhysicalDevice physicalDevice;
VkPhysicalDeviceProperties physicalDeviceProperties{};
VkMemoryRequirements memoryRequirements{};

struct {
    VkQueue graphics, compute, present;
} queues;

struct {
    VkSwapchainKHR swapchain;
    std::vector<VkImage> images;
    int numImages = 0;
    VkFormat format;
    VkExtent2D extent;
} swapchain;

VkPhysicalDeviceRayTracingPropertiesNV rayTracingProperties{};
Vk::BufferHostVisible shaderBindingTable;

VkPipeline pipeline;
VkPipelineLayout pipelineLayout;

VkCommandPool commandPool;

VkDescriptorPool descriptorPoolRender;
VkDescriptorSetLayout descriptorSetLayoutRender, descriptorSetLayoutModels;

struct _PerSwapchainImage {
    VkCommandBuffer commandBufferImageCopy[MAX_FRAMES_IN_FLIGHT];
    VkFence renderCompleteFenceReference; // do not allocate
};
std::vector<_PerSwapchainImage> perSwapchainImage;

struct _PerFrame {
    Vk::StorageImage renderImage;
    VkCommandBuffer commandBufferRender;
    bool rerecordRenderCommands = false;

    Vk::AccelerationStructure tlas;
    VkDescriptorSet descriptorSetModels, descriptorSetRender;
    Vk::BufferDeviceLocal spheresBuffer;

    bool updateEllipsoidTLAS = false;
    std::vector<Model::EllipsoidID> updateEllipsoidIDs;

    Vk::StorageImage objectIDsImage;

    VkSemaphore semaphoreImageAvailable, semaphoreRenderFinished, semaphoreImGuiFinished, semaphoreImageCopyFinished;
    VkFence fenceRenderComplete;
};
_PerFrame perFrame[MAX_FRAMES_IN_FLIGHT];
size_t currentFrame = 0;

Vk::BufferHostVisible bufferUBO; // per frame

std::vector<Model::EllipsoidID> ellipsoidIDs;
std::vector<Vk::AccelerationStructure> ellipsoidBLASs; // one ellipsoid per blas
std::vector<Vk::BLASInstance> ellipsoidInstances;

VkDescriptorPool descriptorPoolModels;

struct UniformData {
    glm::mat4 viewInverse = glm::mat4(1.0f);
    glm::mat4 projInverse = glm::mat4(1.0f);
    glm::vec4 cameraPos = glm::vec4(0.0f);
};

const char* validationLayers[1] = {
    "VK_LAYER_KHRONOS_validation"
};
const char* deviceExtensions[3] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_NV_RAY_TRACING_EXTENSION_NAME,
    VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME
};
const char* instanceExtensions[1] = {
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
};

// rtx function pointers
PFN_vkCreateAccelerationStructureNV vkCreateAccelerationStructureNV;
PFN_vkDestroyAccelerationStructureNV vkDestroyAccelerationStructureNV;
PFN_vkBindAccelerationStructureMemoryNV vkBindAccelerationStructureMemoryNV;
PFN_vkGetAccelerationStructureHandleNV vkGetAccelerationStructureHandleNV;
PFN_vkGetAccelerationStructureMemoryRequirementsNV vkGetAccelerationStructureMemoryRequirementsNV;
PFN_vkCmdBuildAccelerationStructureNV vkCmdBuildAccelerationStructureNV;
PFN_vkCreateRayTracingPipelinesNV vkCreateRayTracingPipelinesNV;
PFN_vkGetRayTracingShaderGroupHandlesNV vkGetRayTracingShaderGroupHandlesNV;
PFN_vkCmdTraceRaysNV vkCmdTraceRaysNV;

#pragma endregion

#pragma region PRIVATE_FUNCTION_DECLARATIONS

// initialization

void createInstance(std::vector<const char*>& requiredExtensions);
void setupDebugMessenger();
void createSurface();
void pickPhysicalDevice();
void createLogicalDevice();

void createSwapChain();
void createCommandPool();
void createSyncObjects();

void createRenderImages();
void initPerFrameRenderResources();
void createUBO(glm::mat4 viewInverse, glm::mat4 projInverse, glm::vec3 cameraPos);

void createDescriptorSetLayouts();
void createRayTracingPipeline();
void createShaderBindingTable();
void createDescriptorSetsRender();

void createCommandBuffersRender();
void createCommandBuffersImageCopy();

// main loop

void updateModels(uint32_t frame);
void updateModelTLAS(uint32_t frame);
void updateEllipsoidBuffer(uint32_t frame);
void updateModelDescriptorSet(uint32_t frame);
void recordCommandBufferRender(uint32_t frame);

void updateUniformBuffer(glm::mat4 viewInverse, glm::mat4 projInverse, glm::vec3 cameraPos, uint32_t frame);

void recreateSwapChain();
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

// clean up

void cleanupSwapChain();
void cleanUpAccelerationStructure(Vk::AccelerationStructure& ac); // todo move to class once we don't need vkGetDeviceProcAddr

// helper

bool checkValidationLayerSupport();
VkResult createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
void testDebugMessenger();

bool isDeviceSuitable(VkPhysicalDevice device);
bool checkDeviceExtensionSupport(VkPhysicalDevice device);
std::vector<const char*> getRequiredExtensions();

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

void recordImageLayoutTransition(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
    VkImageSubresourceRange subresourceRange, VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

VkDeviceSize getUBOOffsetAligned(VkDeviceSize stride);

// todo move to VkHelper after switching to khr ray tracing
void createBLAS(Vk::AccelerationStructure& blas, Vk::AABB aabb);
Vk::BLASInstance createInstance(uint64_t blasHandle);
void createTopLevelAccelerationStructure(Vk::AccelerationStructure& tlas, uint32_t instanceCount);

#pragma endregion

#pragma region FUNCTION_IMPLIMENTATIONS

VkDevice getDevice() { return device; }
VkPhysicalDevice getPhysicalDevice() { return physicalDevice; }
VkQueue getGraphicsQueue() { return queues.graphics; }
VkSurfaceKHR getSurface() { return surface; }
VkCommandPool getCommandPool() { return commandPool; }
uint32_t getNumSwapchainImages() { return swapchain.numImages; }
Vk::StorageImage getRenderImage(uint32_t frame) { return perFrame[frame].renderImage; }

void init(std::vector<const char*>& requiredExtensions, glm::mat4 viewInverse, glm::mat4 projInverse, glm::vec3 cameraPos) {
    AID_INFO("Initializing vulkan renderer...");

    createInstance(requiredExtensions);
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();

    createSwapChain();
    createCommandPool();
    createSyncObjects();

    createRenderImages();
    createDescriptorSetLayouts();
    initPerFrameRenderResources();
    createUBO(viewInverse, projInverse, cameraPos);

    createRayTracingPipeline();
    createShaderBindingTable();
    createDescriptorSetsRender();

    createCommandBuffersRender();
    createCommandBuffersImageCopy();
}

void createInstance(std::vector<const char*>& requiredExtensions) {
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
        createInfo.enabledLayerCount = ARRAY_SIZE(validationLayers);
        createInfo.ppEnabledLayerNames = validationLayers;

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

void setupDebugMessenger() {
    if (!enableValidationLayers) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;

    VK_CHECK_RESULT(createDebugUtilsMessengerEXT(instance, &createInfo, VK_ALLOCATOR, &debugMessenger), "failed to set up debug messenger!");
}

void createSurface() {
    VK_CHECK_RESULT(IOInterface::createVkSurface(instance, VK_ALLOCATOR, &surface), "failed to create window surface!");
}

void pickPhysicalDevice() {
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

void createLogicalDevice() {
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

    createInfo.enabledExtensionCount = ARRAY_SIZE(deviceExtensions);
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = ARRAY_SIZE(validationLayers);
        createInfo.ppEnabledLayerNames = validationLayers;
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

void createSwapChain() {
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

    perSwapchainImage.resize(imageCount);
}

void createCommandPool() {
    Vk::QueueFamilyIndices queueFamilyIndices = Vk::findQueueFamilies(physicalDevice, surface);

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK_RESULT(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool), "failed to create graphics command pool!");
}

void createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &perFrame[f].semaphoreImageAvailable) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &perFrame[f].semaphoreRenderFinished) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &perFrame[f].semaphoreImGuiFinished) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &perFrame[f].semaphoreImageCopyFinished) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &perFrame[f].fenceRenderComplete) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

void createRenderImages() {

    VkImageCreateInfo imageCI = {};
    imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.extent.depth = 1;
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;


    VkImageViewCreateInfo colorImageView{};
    colorImageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    colorImageView.subresourceRange = {};
    colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorImageView.subresourceRange.baseMipLevel = 0;
    colorImageView.subresourceRange.levelCount = 1;
    colorImageView.subresourceRange.baseArrayLayer = 0;
    colorImageView.subresourceRange.layerCount = 1;

    for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
        perFrame[f].renderImage.extent = swapchain.extent;
        perFrame[f].renderImage.format = swapchain.format;

        imageCI.format = perFrame[f].renderImage.format;
        imageCI.extent.width = perFrame[f].renderImage.extent.width;
        imageCI.extent.height = perFrame[f].renderImage.extent.height;

        VK_CHECK_RESULT(vkCreateImage(device, &imageCI, VK_ALLOCATOR, &perFrame[f].renderImage.image), "failed to create ray tracing storage image");

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device, perFrame[f].renderImage.image, &memReqs);
        VkMemoryAllocateInfo memoryAllocateInfo{};
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.allocationSize = memReqs.size;
        memoryAllocateInfo.memoryTypeIndex = Vk::findMemoryType(physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &perFrame[f].renderImage.memory), "failed to allocate render image memory");
        VK_CHECK_RESULT(vkBindImageMemory(device, perFrame[f].renderImage.image, perFrame[f].renderImage.memory, 0), "failed to bind image memory");

        colorImageView.format = perFrame[f].renderImage.format;
        colorImageView.image = perFrame[f].renderImage.image;
        VK_CHECK_RESULT(vkCreateImageView(device, &colorImageView, nullptr, &perFrame[f].renderImage.view), "failed to create render image view");

        VkCommandBuffer cmdBuffer = Vk::beginSingleTimeCommands(device, commandPool);
        recordImageLayoutTransition(cmdBuffer, perFrame[f].renderImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
        Vk::endSingleTimeCommands(device, cmdBuffer, queues.graphics, commandPool);
    }
}

void initPerFrameRenderResources() {

    // create descriptor pool

    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, static_cast<uint32_t>(perSwapchainImage.size()) },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(perSwapchainImage.size()) }
    };

    VkDescriptorPoolCreateInfo descriptorPoolCI{};
    descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolCI.pPoolSizes = poolSizes.data();
    descriptorPoolCI.maxSets = static_cast<uint32_t>(perSwapchainImage.size());
    VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCI, VK_ALLOCATOR, &descriptorPoolModels), "failed to create descriptor pool");

    for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
        // create tlas

        updateModelTLAS(f);

        // init ellipsoids buffer

        perFrame[f].spheresBuffer.create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(Model::Ellipsoid) * SPHERE_COUNT_PER_TLAS, device, physicalDevice);

        // create descriptor set

        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
        descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocateInfo.descriptorPool = descriptorPoolModels;
        descriptorSetAllocateInfo.descriptorSetCount = 1;
        descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayoutModels;
        vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &perFrame[f].descriptorSetModels);
        
        updateModelDescriptorSet(f);
    }
}

void createUBO(glm::mat4 viewInverse, glm::mat4 projInverse, glm::vec3 cameraPos) {
    bufferUBO.dynamicStride = getUBOOffsetAligned(sizeof(UniformData));
    bufferUBO.create(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, static_cast<VkDeviceSize>(MAX_FRAMES_IN_FLIGHT) * bufferUBO.dynamicStride, device, physicalDevice);
    for (int s = 0; s < MAX_FRAMES_IN_FLIGHT; s++) updateUniformBuffer(viewInverse, projInverse, cameraPos, s);
}

void createDescriptorSetLayouts() {
    {
        VkDescriptorSetLayoutBinding layoutBindingRenderImage{};
        layoutBindingRenderImage.binding = 0;
        layoutBindingRenderImage.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        layoutBindingRenderImage.descriptorCount = 1;
        layoutBindingRenderImage.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;

        VkDescriptorSetLayoutBinding layoutBindingUniformBuffer{};
        layoutBindingUniformBuffer.binding = 1;
        layoutBindingUniformBuffer.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        layoutBindingUniformBuffer.descriptorCount = 1;
        layoutBindingUniformBuffer.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;

        std::vector<VkDescriptorSetLayoutBinding> bindings({
            layoutBindingRenderImage,
            layoutBindingUniformBuffer });

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI{};
        descriptorSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCI.bindingCount = static_cast<uint32_t>(bindings.size());
        descriptorSetLayoutCI.pBindings = bindings.data();
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, VK_ALLOCATOR, &descriptorSetLayoutRender), "failed to create descriptor set layout");
    }

    {
        VkDescriptorSetLayoutBinding layoutBindingAccelerationStructure{};
        layoutBindingAccelerationStructure.binding = 0;
        layoutBindingAccelerationStructure.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
        layoutBindingAccelerationStructure.descriptorCount = 1;
        layoutBindingAccelerationStructure.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

        VkDescriptorSetLayoutBinding layoutBindingSpheresBuffer{};
        layoutBindingSpheresBuffer.binding = 1;
        layoutBindingSpheresBuffer.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        layoutBindingSpheresBuffer.descriptorCount = 1;
        layoutBindingSpheresBuffer.stageFlags = VK_SHADER_STAGE_INTERSECTION_BIT_NV;

        std::vector<VkDescriptorSetLayoutBinding> bindings({
            layoutBindingAccelerationStructure,
            layoutBindingSpheresBuffer });

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI{};
        descriptorSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCI.bindingCount = static_cast<uint32_t>(bindings.size());
        descriptorSetLayoutCI.pBindings = bindings.data();
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, VK_ALLOCATOR, &descriptorSetLayoutModels), "failed to create descriptor set layout");
    }
}

void createRayTracingPipeline() {
    VkDescriptorSetLayout descriptorLayouts[] = { descriptorSetLayoutRender, descriptorSetLayoutModels };

    VkPipelineLayoutCreateInfo pipelineLayoutCI{};
    pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCI.setLayoutCount = 2;
    pipelineLayoutCI.pSetLayouts = descriptorLayouts;
    vkCreatePipelineLayout(device, &pipelineLayoutCI, VK_ALLOCATOR, &pipelineLayout);

    VkShaderModule shaderModules[STAGE_COUNT] = {};
    VkPipelineShaderStageCreateInfo shaderStages[STAGE_COUNT] = {};
    shaderStages[STAGE_RAYGEN]                  = Vk::loadShader(device, std::string(_CONFIG::getAssetsPath()) + std::string(SHADER_SRC_RAYGEN),                 VK_SHADER_STAGE_RAYGEN_BIT_NV,       shaderModules[STAGE_RAYGEN]);
    shaderStages[STAGE_MISS_BACKGROUND]         = Vk::loadShader(device, std::string(_CONFIG::getAssetsPath()) + std::string(SHADER_SRC_MISS_BACKGROUND),        VK_SHADER_STAGE_MISS_BIT_NV,         shaderModules[STAGE_MISS_BACKGROUND]);
    shaderStages[STAGE_MISS_SHADOW]             = Vk::loadShader(device, std::string(_CONFIG::getAssetsPath()) + std::string(SHADER_SRC_MISS_SHADOW),            VK_SHADER_STAGE_MISS_BIT_NV,         shaderModules[STAGE_MISS_SHADOW]);
    shaderStages[STAGE_CLOSEST_HIT_SCENE]       = Vk::loadShader(device, std::string(_CONFIG::getAssetsPath()) + std::string(SHADER_SRC_CLOSEST_HIT_SCENE),      VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV,  shaderModules[STAGE_CLOSEST_HIT_SCENE]);
    shaderStages[STAGE_INTERSECTION_ELLIPSOID]  = Vk::loadShader(device, std::string(_CONFIG::getAssetsPath()) + std::string(SHADER_SRC_INTERSECTION_ELLIPSOID), VK_SHADER_STAGE_INTERSECTION_BIT_NV, shaderModules[STAGE_INTERSECTION_ELLIPSOID]);

    // ray tracing shader groups
    VkRayTracingShaderGroupCreateInfoNV shaderGroups[GROUP_COUNT] = {};
    for (auto& group : shaderGroups) {
        group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV;
        group.generalShader = VK_SHADER_UNUSED_NV;
        group.closestHitShader = VK_SHADER_UNUSED_NV;
        group.anyHitShader = VK_SHADER_UNUSED_NV;
        group.intersectionShader = VK_SHADER_UNUSED_NV;
    }
    
    // ray generation
    shaderGroups[GROUP_RAYGEN].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
    shaderGroups[GROUP_RAYGEN].generalShader = STAGE_RAYGEN;

    // scene miss
    shaderGroups[GROUP_MISS_BACKGROUND].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
    shaderGroups[GROUP_MISS_BACKGROUND].generalShader = STAGE_MISS_BACKGROUND;

    // shadow miss
    shaderGroups[GROUP_MISS_SHADOW].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
    shaderGroups[GROUP_MISS_SHADOW].generalShader = STAGE_MISS_SHADOW;

    // scene closest hit
    shaderGroups[GROUP_HIT_SCENE].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_NV;
    shaderGroups[GROUP_HIT_SCENE].closestHitShader = STAGE_CLOSEST_HIT_SCENE;
    shaderGroups[GROUP_HIT_SCENE].intersectionShader = STAGE_INTERSECTION_ELLIPSOID;

    VkRayTracingPipelineCreateInfoNV rayPipelineCI{};
    rayPipelineCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV;
    rayPipelineCI.stageCount = ARRAY_SIZE(shaderStages);
    rayPipelineCI.pStages = shaderStages;
    rayPipelineCI.groupCount = ARRAY_SIZE(shaderGroups);
    rayPipelineCI.pGroups = shaderGroups;
    rayPipelineCI.maxRecursionDepth = 2;
    rayPipelineCI.layout = pipelineLayout;
    VK_CHECK_RESULT(vkCreateRayTracingPipelinesNV(device, VK_NULL_HANDLE, 1, &rayPipelineCI, VK_ALLOCATOR, &pipeline), "failed to create ray tracing pipeline");

    for (VkShaderModule shaderModule : shaderModules)
        vkDestroyShaderModule(device, shaderModule, VK_ALLOCATOR);
}

void createShaderBindingTable() {
    uint32_t sbtSize = rayTracingProperties.shaderGroupHandleSize * GROUP_COUNT;
    uint8_t* shaderGroupHandleStorage = new uint8_t[sbtSize];
    VK_CHECK_RESULT(vkGetRayTracingShaderGroupHandlesNV(device, pipeline, 0, GROUP_COUNT, sbtSize, shaderGroupHandleStorage), "failed to get ray tracing shader group handles");

    shaderBindingTable.create(VK_BUFFER_USAGE_RAY_TRACING_BIT_NV | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sbtSize, device, physicalDevice);
    shaderBindingTable.upload(shaderGroupHandleStorage, sbtSize, 0, device);
}

void createDescriptorSetsRender() {
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, MAX_FRAMES_IN_FLIGHT }
    };

    VkDescriptorPoolCreateInfo descriptorPoolCI{};
    descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolCI.pPoolSizes = poolSizes.data();
    descriptorPoolCI.maxSets = MAX_FRAMES_IN_FLIGHT;
    VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCI, VK_ALLOCATOR, &descriptorPoolRender), "failed to create descriptor pool");

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = descriptorPoolRender;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayoutRender;

    // render image descriptor

    VkDescriptorImageInfo renderImageDescriptor[MAX_FRAMES_IN_FLIGHT];

    VkWriteDescriptorSet renderImageWrite{};
    renderImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    renderImageWrite.descriptorCount = 1;
    renderImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    renderImageWrite.dstBinding = 0;

    // ubo descriptor

    VkDescriptorBufferInfo uboDescriptor{};
    uboDescriptor.buffer = bufferUBO.buffer;
    uboDescriptor.offset = 0;
    uboDescriptor.range = bufferUBO.dynamicStride;

    VkWriteDescriptorSet uniformBufferWrite{};
    uniformBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    uniformBufferWrite.descriptorCount = 1;
    uniformBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    uniformBufferWrite.pBufferInfo = &uboDescriptor;
    uniformBufferWrite.dstBinding = 1;

    std::vector<VkWriteDescriptorSet> writeDescriptorSets;
    for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
        VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &perFrame[f].descriptorSetRender), "failed to allocate render descriptor set");

        renderImageDescriptor[f] = {};
        renderImageDescriptor[f].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        renderImageDescriptor[f].imageView = perFrame[f].renderImage.view;

        renderImageWrite.pImageInfo = &renderImageDescriptor[f];
        renderImageWrite.dstSet = perFrame[f].descriptorSetRender;
        writeDescriptorSets.push_back(renderImageWrite);

        uniformBufferWrite.dstSet = perFrame[f].descriptorSetRender;
        writeDescriptorSets.push_back(uniformBufferWrite);
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
}

void createCommandBuffersRender() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandBufferCount = 1;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
        vkAllocateCommandBuffers(device, &allocInfo, &perFrame[f].commandBufferRender);
        recordCommandBufferRender(f);
    }
}

void createCommandBuffersImageCopy() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    for (int s = 0; s < perSwapchainImage.size(); s++)
        vkAllocateCommandBuffers(device, &allocInfo, perSwapchainImage[s].commandBufferImageCopy);

    // begin info
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    for (int s = 0; s < swapchain.numImages; s++) {
        for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
            VkCommandBuffer& commandBuffer = perSwapchainImage[s].commandBufferImageCopy[f];
            VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &beginInfo), "failed to begin command buffer");

            // copy ray tracing output to swapchain image

            recordImageLayoutTransition(
                commandBuffer,
                swapchain.images[s],
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                subresourceRange);

            recordImageLayoutTransition(
                commandBuffer,
                perFrame[f].renderImage.image,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                subresourceRange);

            VkImageCopy copyRegion{};
            copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            copyRegion.srcOffset = { 0, 0, 0 };
            copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            copyRegion.dstOffset = { 0, 0, 0 };
            copyRegion.extent = { swapchain.extent.width, swapchain.extent.height, 1 };
            vkCmdCopyImage(commandBuffer, perFrame[f].renderImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchain.images[s], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            recordImageLayoutTransition(
                commandBuffer,
                swapchain.images[s],
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                subresourceRange);

            recordImageLayoutTransition(
                commandBuffer,
                perFrame[f].renderImage.image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_GENERAL,
                subresourceRange);

            VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer), "failed to end rendering command buffer {}", s);
        }
    }
}

// MAIN LOOP

void drawFrame(bool framebufferResized, glm::mat4 viewInverse, glm::mat4 projInverse, glm::vec3 cameraPos, bool renderImGui) {
    vkWaitForFences(device, 1, &perFrame[currentFrame].fenceRenderComplete, VK_TRUE, DEFAULT_FENCE_TIMEOUT);

    uint32_t imageIndex;
    VkResult resultAcquire = vkAcquireNextImageKHR(device, swapchain.swapchain, UINT64_MAX, perFrame[currentFrame].semaphoreImageAvailable, VK_NULL_HANDLE, &imageIndex);

    // TODO framebufferResized? debug and check result values
    if (resultAcquire == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        ImGuiVk::recreateFramebuffers();
        return;
    } else if (resultAcquire != VK_SUCCESS && resultAcquire != VK_SUBOPTIMAL_KHR) {
        AID_ERROR("failed to acquire swap chain image!");
    }

    updateModels(currentFrame);
    if (perFrame[currentFrame].rerecordRenderCommands) {
        recordCommandBufferRender(currentFrame);
        perFrame[currentFrame].rerecordRenderCommands = false;
    }
    updateUniformBuffer(viewInverse, projInverse, cameraPos, currentFrame);

    if (perSwapchainImage[imageIndex].renderCompleteFenceReference != VK_NULL_HANDLE) {
        vkWaitForFences(device, 1, &perSwapchainImage[imageIndex].renderCompleteFenceReference, VK_TRUE, DEFAULT_FENCE_TIMEOUT);
    }
    perSwapchainImage[imageIndex].renderCompleteFenceReference = perFrame[currentFrame].fenceRenderComplete;

    // ray tracing dispatch
    {
        VkSemaphore signalSemaphores[] = { perFrame[currentFrame].semaphoreRenderFinished };

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &perFrame[currentFrame].commandBufferRender;
        submitInfo.waitSemaphoreCount = 0;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VK_CHECK_RESULT(vkQueueSubmit(queues.graphics, 1, &submitInfo, VK_NULL_HANDLE), "failed to submit render queue {}", imageIndex);
    }

    // render imGui
    if (renderImGui) ImGuiVk::recordRenderCommands(currentFrame);
    renderImGui &= ImGuiVk::shouldRender(currentFrame);
    if (renderImGui) {

        VkSemaphore waitSemaphores[] = { perFrame[currentFrame].semaphoreRenderFinished };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSemaphore signalSemaphores[] = { perFrame[currentFrame].semaphoreImGuiFinished };

        VkCommandBuffer commandBuffers[] = { ImGuiVk::getCommandBuffer(currentFrame) };

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
        VkSemaphore signalSemaphores[] = { perFrame[currentFrame].semaphoreImageCopyFinished };
        VkSemaphore waitSemaphores[2];
        waitSemaphores[0] = perFrame[currentFrame].semaphoreImageAvailable;
        if (renderImGui) waitSemaphores[1] = perFrame[currentFrame].semaphoreImGuiFinished;
        else waitSemaphores[1] = perFrame[currentFrame].semaphoreRenderFinished;
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT };

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &perSwapchainImage[imageIndex].commandBufferImageCopy[currentFrame];
        submitInfo.waitSemaphoreCount = 2;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkResetFences(device, 1, &perFrame[currentFrame].fenceRenderComplete);
        VK_CHECK_RESULT(vkQueueSubmit(queues.graphics, 1, &submitInfo, perFrame[currentFrame].fenceRenderComplete), "failed to submit render queue {}", imageIndex);
    }

    // present
    {
        VkSemaphore waitSemaphores[] = { perFrame[currentFrame].semaphoreImageCopyFinished };

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
            ImGuiVk::recreateFramebuffers();
        } else if (resultPresent != VK_SUCCESS) {
            AID_ERROR("failed to present swap chain image!");
        }
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

int addEllipsoid(Model::EllipsoidID ellipsoidID) {
    // preset limit for now
    if (SPHERE_COUNT_PER_TLAS <= ellipsoidIDs.size()) return -1;

    uint32_t ellipsoidIndex = ellipsoidIDs.size();
    ellipsoidIDs.push_back(ellipsoidID);
    ellipsoidBLASs.push_back(Vk::AccelerationStructure());

    // add a BLAS

    Vk::AABB sphereAABB(PrimitiveManager::getEllipsoid(ellipsoidID));
    createBLAS(ellipsoidBLASs[ellipsoidIndex], sphereAABB);

    // add instance

    ellipsoidInstances.push_back(createInstance(ellipsoidBLASs[ellipsoidIndex].handle));

    // signal that a tlas update is required

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        perFrame[i].updateEllipsoidTLAS = true;
        perFrame[i].updateEllipsoidIDs.push_back(ellipsoidID);
    }
    return 0;
}

int updateEllipsoid(Model::EllipsoidID ellipsoidID) {

    int ellipsoidIndex = Model::containsID(ellipsoidIDs, ellipsoidID);
    if (ellipsoidIndex == -1) {
        AID_WARN("Renderer::updateEllipsoid() tried to update ellpisoid {} that hasn't been added", ellipsoidID.getID());
        return -1;
    }

    // recreate BLAS

    cleanUpAccelerationStructure(ellipsoidBLASs[ellipsoidIndex]);

    Vk::AABB sphereAABB(PrimitiveManager::getEllipsoid(ellipsoidID));
    createBLAS(ellipsoidBLASs[ellipsoidIndex], sphereAABB);

    // recreate instance

    ellipsoidInstances[ellipsoidIndex] = createInstance(ellipsoidBLASs[ellipsoidIndex].handle);

    // signal that a tlas update is required

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        perFrame[i].updateEllipsoidTLAS = true;
        perFrame[i].updateEllipsoidIDs.push_back(ellipsoidID);
    }
    return 0;
}

int removeEllipsoid(Model::EllipsoidID ellipsoidID) {

    int ellipsoidIndex = Model::containsID(ellipsoidIDs, ellipsoidID);
    if (ellipsoidIndex == -1) {
        AID_WARN("Renderer::removeEllipsoid() tried to remove ellpisoid {} that hasn't been added", ellipsoidID.getID());
        return -1;
    }

    cleanUpAccelerationStructure(ellipsoidBLASs[ellipsoidIndex]);

    ellipsoidBLASs.erase(ellipsoidBLASs.begin() + ellipsoidIndex);
    ellipsoidInstances.erase(ellipsoidInstances.begin() + ellipsoidIndex);
    ellipsoidIDs.erase(ellipsoidIDs.begin() + ellipsoidIndex);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        perFrame[i].updateEllipsoidTLAS = true;
        for (int i = ellipsoidIndex; i < ellipsoidIDs.size(); i++)
            perFrame[i].updateEllipsoidIDs.push_back(ellipsoidIDs[i]);
    }
    return 0;
}

void updateModels(uint32_t frame) {
    updateEllipsoidBuffer(frame);

    if (!perFrame[frame].updateEllipsoidTLAS) return;

    vkFreeMemory(device, perFrame[frame].tlas.memory, VK_ALLOCATOR);
    vkDestroyAccelerationStructureNV(device, perFrame[frame].tlas.accelerationStructure, nullptr);
    updateModelTLAS(frame);

    updateModelDescriptorSet(frame);
    perFrame[frame].rerecordRenderCommands = true;

    perFrame[frame].updateEllipsoidTLAS = false;
}

void updateModelTLAS(uint32_t frame) {
    Vk::AccelerationStructure& tlas = perFrame[frame].tlas;

    Vk::BufferHostVisible instanceBuffer;

    if (ellipsoidInstances.size() != 0) {
        instanceBuffer.create(VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, sizeof(Vk::BLASInstance) * ellipsoidInstances.size(), device, physicalDevice);
        instanceBuffer.upload(ellipsoidInstances.data(), sizeof(Vk::BLASInstance) * ellipsoidInstances.size(), 0, device);
    }

    // create top-level acceleration structure
    // todo: don't have to recreate! create with max_spheres
    createTopLevelAccelerationStructure(tlas, ellipsoidInstances.size());

    // acceleration structure building requires some scratch space to store temporary information

    VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo{};
    memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
    memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;

    VkMemoryRequirements2 memReqTopLevelAS;
    memoryRequirementsInfo.accelerationStructure = tlas.accelerationStructure;
    vkGetAccelerationStructureMemoryRequirementsNV(device, &memoryRequirementsInfo, &memReqTopLevelAS);

    Vk::BufferDeviceLocal scratchBuffer;
    scratchBuffer.create(VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, memReqTopLevelAS.memoryRequirements.size, device, physicalDevice);

    // build top-level acceleration structure

    VkAccelerationStructureInfoNV buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
    buildInfo.geometryCount = 0;
    buildInfo.pGeometries = nullptr;
    buildInfo.instanceCount = ellipsoidInstances.size();

    VkCommandBuffer cmdBuffer = Vk::beginSingleTimeCommands(device, commandPool);

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

    Vk::endSingleTimeCommands(device, cmdBuffer, queues.compute, commandPool);

    instanceBuffer.destroy(device);
    scratchBuffer.destroy(device);
}

void updateEllipsoidBuffer(uint32_t frame) {
    for (Model::EllipsoidID ellipsoidID : perFrame[frame].updateEllipsoidIDs) {

        int ellipsoidIndex = Model::containsID(ellipsoidIDs, ellipsoidID);
        if (ellipsoidIndex == -1) {
            AID_ERROR("Renderer::updateEllipsoidBuffer() attempting to update ellipsoid id {} that hasn't been added", ellipsoidID.getID());
        }

        Model::Ellipsoid ellipsoid = PrimitiveManager::getEllipsoid(ellipsoidIDs[ellipsoidIndex]);

        perFrame[frame].spheresBuffer.upload(&ellipsoid, sizeof(Model::Ellipsoid), sizeof(Model::Ellipsoid) * ellipsoidIndex,
            device, physicalDevice, queues.graphics, commandPool);
    }

    perFrame[frame].updateEllipsoidIDs.clear();
}

void updateModelDescriptorSet(uint32_t frame) {
    VkDescriptorSet& descriptorSet = perFrame[frame].descriptorSetModels;

    // acceleration structure descriptor

    VkWriteDescriptorSetAccelerationStructureNV descriptorAccelerationStructureInfo{};
    descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV;
    descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
    descriptorAccelerationStructureInfo.pAccelerationStructures = &perFrame[frame].tlas.accelerationStructure;

    VkWriteDescriptorSet accelerationStructureWrite{};
    accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    // The specialized acceleration structure descriptor has to be chained
    accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
    accelerationStructureWrite.dstSet = descriptorSet;
    accelerationStructureWrite.descriptorCount = 1;
    accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
    accelerationStructureWrite.dstBinding = 0;

    // sphere ssbo

    VkDescriptorBufferInfo spheresDescriptor{};
    spheresDescriptor.buffer = perFrame[frame].spheresBuffer.buffer;
    spheresDescriptor.offset = 0;
    spheresDescriptor.range = perFrame[frame].spheresBuffer.size;

    VkWriteDescriptorSet spheresWrite{};
    spheresWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    spheresWrite.dstSet = descriptorSet;
    spheresWrite.descriptorCount = 1;
    spheresWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    spheresWrite.pBufferInfo = &spheresDescriptor;
    spheresWrite.dstBinding = 1;

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        accelerationStructureWrite,
        spheresWrite
    };
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
}

void recordCommandBufferRender(uint32_t frame) {
    // shader binding offsets
    VkDeviceSize bindingOffsetRayGenShader = static_cast<VkDeviceSize>(rayTracingProperties.shaderGroupHandleSize) * GROUP_RAYGEN;
    VkDeviceSize bindingOffsetMissShader   = static_cast<VkDeviceSize>(rayTracingProperties.shaderGroupHandleSize) * GROUP_MISS_BACKGROUND;
    VkDeviceSize bindingOffsetHitShader    = static_cast<VkDeviceSize>(rayTracingProperties.shaderGroupHandleSize) * GROUP_HIT_SCENE;
    VkDeviceSize bindingStride = static_cast<VkDeviceSize>(rayTracingProperties.shaderGroupHandleSize);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    VkCommandBuffer& commandBuffer = perFrame[frame].commandBufferRender;
    VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &beginInfo), "failed to begin command buffer");

    // ray tracing dispath

    uint32_t uboDynamicOffset = frame * bufferUBO.dynamicStride;

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, pipelineLayout, 0, 1, &perFrame[frame].descriptorSetRender, 1, &uboDynamicOffset);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, pipelineLayout, 1, 1, &perFrame[frame].descriptorSetModels, 0, nullptr);

    vkCmdTraceRaysNV(commandBuffer,
        shaderBindingTable.buffer, bindingOffsetRayGenShader,
        shaderBindingTable.buffer, bindingOffsetMissShader, bindingStride,
        shaderBindingTable.buffer, bindingOffsetHitShader, bindingStride,
        VK_NULL_HANDLE, 0, 0,
        swapchain.extent.width, swapchain.extent.height, 1);

    VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer), "failed to end rendering command buffer");
}

void updateUniformBuffer(glm::mat4 viewInverse, glm::mat4 projInverse, glm::vec3 cameraPos, uint32_t frame) {
    UniformData uniformData;
    uniformData.viewInverse = viewInverse;
    uniformData.projInverse = projInverse;
    uniformData.cameraPos = glm::vec4(cameraPos, 1.0f);

    bufferUBO.upload(&uniformData, sizeof(UniformData), static_cast<VkDeviceSize>(frame) * bufferUBO.dynamicStride, device);
}

void recreateSwapChain() {
    vkDeviceWaitIdle(device);

    cleanupSwapChain();

    createSwapChain();
    createRenderImages();
    createDescriptorSetsRender();
    createCommandBuffersRender();
    createCommandBuffersImageCopy();
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
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

void cleanUp() {
    vkDeviceWaitIdle(device);
    AID_INFO("Cleaning up Vulkan renderer...");

    vkDestroyPipeline(device, pipeline, VK_ALLOCATOR);
    vkDestroyPipelineLayout(device, pipelineLayout, VK_ALLOCATOR);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayoutModels, VK_ALLOCATOR);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayoutRender, VK_ALLOCATOR);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        perFrame[i].spheresBuffer.destroy(device);
        vkDestroyAccelerationStructureNV(device, perFrame[i].tlas.accelerationStructure, nullptr);
        vkFreeMemory(device, perFrame[i].tlas.memory, VK_ALLOCATOR);
    }
    perSwapchainImage.clear();

    ellipsoidIDs.clear();
    for (Vk::AccelerationStructure& as : ellipsoidBLASs) cleanUpAccelerationStructure(as);
    ellipsoidBLASs.clear();
    ellipsoidInstances.clear();

    bufferUBO.destroy(device);
    shaderBindingTable.destroy(device);
    vkDestroyDescriptorPool(device, descriptorPoolModels, VK_ALLOCATOR);

    cleanupSwapChain();

    for (size_t f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
        vkDestroySemaphore(device, perFrame[f].semaphoreImageAvailable, VK_ALLOCATOR);
        vkDestroySemaphore(device, perFrame[f].semaphoreRenderFinished, VK_ALLOCATOR);
        vkDestroySemaphore(device, perFrame[f].semaphoreImGuiFinished, VK_ALLOCATOR);
        vkDestroySemaphore(device, perFrame[f].semaphoreImageCopyFinished, VK_ALLOCATOR);
        vkDestroyFence(device, perFrame[f].fenceRenderComplete, VK_ALLOCATOR);
    }
    vkDestroyCommandPool(device, commandPool, VK_ALLOCATOR);

    perSwapchainImage.clear();
    vkDestroyDevice(device, VK_ALLOCATOR);

    if (enableValidationLayers)
        destroyDebugUtilsMessengerEXT(instance, debugMessenger, VK_ALLOCATOR);

    vkDestroySurfaceKHR(instance, surface, VK_ALLOCATOR);
    vkDestroyInstance(instance, VK_ALLOCATOR);
}

void cleanupSwapChain() {
    vkDestroySwapchainKHR(device, swapchain.swapchain, VK_ALLOCATOR);
    for (int s = 0; s < perSwapchainImage.size(); s++) {
        vkFreeCommandBuffers(device, commandPool, MAX_FRAMES_IN_FLIGHT, perSwapchainImage[s].commandBufferImageCopy);
    }
    for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
        vkFreeCommandBuffers(device, commandPool, 1, &perFrame[f].commandBufferRender);
        perFrame[f].renderImage.destroy(device);
    }
    vkDestroyDescriptorPool(device, descriptorPoolRender, VK_ALLOCATOR);
}

void cleanUpAccelerationStructure(Vk::AccelerationStructure& as) {
    if (as.memory) vkFreeMemory(device, as.memory, VK_ALLOCATOR);
    if (as.accelerationStructure) Renderer::vkDestroyAccelerationStructureNV(device, as.accelerationStructure, nullptr);
}

// HELPER FUNCTIONS

bool checkValidationLayerSupport() {
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

VkResult createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

void testDebugMessenger() {
    VkDebugUtilsMessengerCallbackDataEXT debugInitMessage = {};
    debugInitMessage.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
    debugInitMessage.pMessageIdName = "Initialization";
    debugInitMessage.messageIdNumber = 0;
    debugInitMessage.pMessage = "Debug messenger initialized!";

    auto func = (PFN_vkSubmitDebugUtilsMessageEXT)vkGetInstanceProcAddr(instance, "vkSubmitDebugUtilsMessageEXT");
    func(instance, VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &debugInitMessage);
}

bool isDeviceSuitable(VkPhysicalDevice device) {
    Vk::QueueFamilyIndices indices = Vk::findQueueFamilies(device, surface);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        Vk::SwapChainSupportDetails swapChainSupport = Vk::querySwapChainSupport(device, surface);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions;

    for (int i = 0; i < ARRAY_SIZE(deviceExtensions); i++)
        requiredExtensions.insert(deviceExtensions[i]);

    for (const auto& extension : availableExtensions)
        requiredExtensions.erase(extension.extensionName);

    return requiredExtensions.empty();
}

std::vector<const char*> getRequiredExtensions() {
    std::vector<const char*> extensions;

    if (enableValidationLayers)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    for (auto extension : instanceExtensions)
        extensions.push_back(extension);

    return extensions;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM) { //availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        int width = 0, height = 0;
        IOInterface::getWindowSize(&width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}

void recordImageLayoutTransition(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
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
        AID_WARN("recordSetImageLayout oldImageLayout not supported!");
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
        AID_WARN("recordSetImageLayout newImageLayout not supported!");
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

VkDeviceSize getUBOOffsetAligned(VkDeviceSize stride) {
    VkDeviceSize minAlignment = physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
    return minAlignment * static_cast<VkDeviceSize>((stride + minAlignment - 1) / minAlignment); // minAlignment * ceil(stride / minAlignment)
}

void createTopLevelAccelerationStructure(Vk::AccelerationStructure& tlas, uint32_t instanceCount) {
    VkAccelerationStructureInfoNV accelerationStructureInfo{};
    accelerationStructureInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
    accelerationStructureInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
    accelerationStructureInfo.instanceCount = instanceCount;
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

void createBLAS(Vk::AccelerationStructure& blas, Vk::AABB aabb) {

    Vk::BufferHostVisible AABBBuffer;
    AABBBuffer.create(VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(Vk::AABB), device, physicalDevice);
    AABBBuffer.upload(&aabb, sizeof(Vk::AABB), 0, device);

    VkGeometryAABBNV geometryAabbSphere = {};
    geometryAabbSphere.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
    geometryAabbSphere.aabbData = AABBBuffer.buffer;
    geometryAabbSphere.numAABBs = 1;
    geometryAabbSphere.stride = sizeof(Vk::AABB);
    geometryAabbSphere.offset = 0;

    VkGeometryNV geometrySphere{};
    geometrySphere.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
    geometrySphere.flags;// = VK_GEOMETRY_OPAQUE_BIT_NV; TODO need this for scene but not for shadows
    geometrySphere.geometryType = VK_GEOMETRY_TYPE_AABBS_NV;
    geometrySphere.geometry.aabbs = geometryAabbSphere;

    geometrySphere.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
    geometrySphere.geometry.triangles.vertexCount = 0;
    geometrySphere.geometry.triangles.indexCount = 0;

    // create BLAS
    {
        VkAccelerationStructureInfoNV accelerationStructureInfo{};
        accelerationStructureInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
        accelerationStructureInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
        accelerationStructureInfo.instanceCount = 0;
        accelerationStructureInfo.geometryCount = 1;
        accelerationStructureInfo.pGeometries = &geometrySphere;

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

    // build the blas
    {
        VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo{};
        memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
        memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;

        VkMemoryRequirements2 blasMemoryrequirements;
        memoryRequirementsInfo.accelerationStructure = blas.accelerationStructure;
        vkGetAccelerationStructureMemoryRequirementsNV(device, &memoryRequirementsInfo, &blasMemoryrequirements);

        Vk::BufferDeviceLocal scratchBuffer;
        scratchBuffer.create(VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, blasMemoryrequirements.memoryRequirements.size, device, physicalDevice);

        VkCommandBuffer commandBufferBuild = Vk::beginSingleTimeCommands(device, commandPool);

        VkAccelerationStructureInfoNV buildInfo{};
        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
        buildInfo.instanceCount = 0;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &geometrySphere;

        vkCmdBuildAccelerationStructureNV(
            commandBufferBuild,
            &buildInfo,
            VK_NULL_HANDLE,
            0,
            VK_FALSE,
            blas.accelerationStructure,
            VK_NULL_HANDLE,
            scratchBuffer.buffer,
            0);

        Vk::endSingleTimeCommands(device, commandBufferBuild, queues.graphics, commandPool);

        scratchBuffer.destroy(device);
    }

    AABBBuffer.destroy(device);
}

Vk::BLASInstance createInstance(uint64_t blasHandle) {

    glm::mat3x4 transform = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };
    //glm::mat4 transform = glm::rotate(glm::mat4(1.0), _AID_PI * 0.25f, glm::vec3(1.0, 0.0, 0.0));

    Vk::BLASInstance instance{};
    instance.transform = transform;
    instance.instanceId = 0;
    instance.mask = 0xff;
    instance.instanceOffset = 0;
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;
    instance.accelerationStructureHandle = blasHandle;

    return instance;
}

#pragma endregion

};