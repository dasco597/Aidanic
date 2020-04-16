#pragma once

#include "tools/VkHelper.h"
#include "ImguiVk.h"
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

class Aidanic;
class ImGuiVk;

class Renderer {
public:
    void init(Aidanic* app, std::vector<const char*>& requiredExtensions, glm::mat4 viewInverse, glm::mat4 projInverse, glm::vec3 cameraPos);
    void drawFrame(bool framebufferResized, glm::mat4 viewInverse, glm::mat4 projInverse, glm::vec3 cameraPos, ImGuiVk* imGuiRenderer, bool renderImGui = false);
    void cleanUp();

    int addEllipsoid(Model::Ellipsoid ellipsoid);
    //int addSphere(Model::Sphere sphere); // returns 0 for success

    VkDevice getDevice() { return device; }
    VkPhysicalDevice getPhysicalDevice() { return physicalDevice; }
    VkQueue getGraphicsQueue() { return queues.graphics; }
    VkSurfaceKHR getSurface() { return surface; }
    VkCommandPool getCommandPool() { return commandPool; }
    uint32_t getNumSwapchainImages() { return swapchain.numImages; }
    Vk::StorageImage getRenderImage() { return renderImage; }

private:
#pragma region VARIABLES

    Aidanic* aidanicApp;

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
    Vk::StorageImage renderImage;
    Vk::BufferHostVisible shaderBindingTable;

    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffersImageCopy;

    VkDescriptorSet descriptorSetRender;
    VkDescriptorPool descriptorPoolRender;
    VkDescriptorSetLayout descriptorSetLayoutRender, descriptorSetLayoutModels;

    Vk::BufferHostVisible bufferUBO;

    uint32_t sphereCount = 0;
    std::array<Model::Ellipsoid, SPHERE_COUNT_PER_TLAS> spheres;
    Vk::BufferDeviceLocal sphereAABBsBuffer;
    std::array<Vk::AccelerationStructure, SPHERE_COUNT_PER_TLAS> sphereBLASs; // one sphere per blas
    std::vector<Vk::BLASInstance> sphereInstances;

    VkDescriptorPool descriptorPoolModels;
    struct PerFrameRenderResources {
        Vk::AccelerationStructure tlas;
        VkDescriptorSet descriptorSetModels;
        Vk::BufferDeviceLocal spheresBuffer;

        bool updateSpheres = false;
        std::vector<uint32_t> updateSphereIndices;
    };
    std::vector<PerFrameRenderResources> perFrameRenderResources;
    std::vector<bool> recordCommandBufferRenderSignals;
    std::vector<VkCommandBuffer> commandBuffersRender;

    struct UniformData {
        glm::mat4 viewInverse = glm::mat4(1.0f);
        glm::mat4 projInverse = glm::mat4(1.0f);
        glm::vec4 cameraPos = glm::vec4(0.0f);
    };

    std::vector<VkSemaphore> semaphoresImageAvailable, semaphoresRenderFinished, semaphoresImGuiFinished, semaphoresImageCopyFinished;
    std::vector<VkFence> inFlightFences, imagesInFlight;
    size_t currentFrame = 0;

    const std::array<const char*, 1> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    const std::array<const char*, 3> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_NV_RAY_TRACING_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME
    };
    const std::array<const char*, 1> instanceExtensions = {
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
    };

#pragma endregion
#pragma region FUNCTIONS

    // initialization

    void createInstance(std::vector<const char*>& requiredExtensions);
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();

    void createSwapChain();
    void createCommandPool();
    void createSyncObjects();

    void createRenderImage();
    void initPerFrameRenderResources();
    void createAABBBuffers();
    void createUBO(glm::mat4 viewInverse, glm::mat4 projInverse, glm::vec3 cameraPos);

    void createDescriptorSetLayouts();
    void createRayTracingPipeline();
    void createShaderBindingTable();
    void createDescriptorSetRender();

    void createCommandBuffersRender();
    void createCommandBuffersImageCopy();

    // main loop

    void updateModels(uint32_t swapchainIndex);
    void updateModelTLAS(uint32_t swapchainIndex);
    void updateSpheresBuffer(uint32_t swapchainIndex);
    void updateModelDescriptorSet(uint32_t swapchainIndex);
    void recordCommandBufferRender(uint32_t swapchainIndex);

    void updateUniformBuffer(glm::mat4 viewInverse, glm::mat4 projInverse, glm::vec3 cameraPos, uint32_t swapchainIndex);
    
    void recreateSwapChain();
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

    // clean up

    void cleanupSwapChain();

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
    void createBottomLevelAccelerationStructure(Vk::AccelerationStructure& blas, const VkGeometryNV* geometries, uint32_t geometryCount);
    void createTopLevelAccelerationStructure(Vk::AccelerationStructure& tlas, uint32_t instanceCount);

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
};

