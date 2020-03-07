#pragma once

/*
    todo: replace error checks with macro or something?
    write rtx code
    write descriptor set allocation system
*/

#include "tools/Log.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>

#include <vector>
#include <optional>
#include <string>

// Check vulkan result macro
#define VK_CHECK_RESULT(result, ...) if (result != VK_SUCCESS) { AID_ERROR(__VA_ARGS__); }

// Vulkan allocator
#define VK_ALLOCATOR nullptr
#define DEFAULT_FENCE_TIMEOUT 100000000000

class IOInterface;

class RendererBase {
public:

protected:
    void initBase(IOInterface* ioInterface);
    void drawFrameBase(bool framebufferResized);
    void cleanUpBase();

    struct Buffer {
        // todo make fully functioning buffer class
        VkBuffer buffer;
        VkDeviceMemory memory;
        VkDeviceSize size = 0;
        void* mapping = nullptr;
    };

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

    IOInterface* ioInterface;
    bool _initialized = false;
    
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;

    struct {
        VkQueue graphics;
        VkQueue present;
    } queues;

    struct {
        VkSwapchainKHR swapchain;
        std::vector<VkImage> images;
        int numImages = 0;
        VkFormat imageFormat;
        VkExtent2D extent;
        std::vector<VkImageView> imageViews;
        std::vector<VkFramebuffer> framebuffers;
    } swapchain;

    uint32_t width = 0, height = 0;

    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;

    VkCommandPool commandPool;

    Buffer vertexBuffer, indexBuffer;
    uint32_t indexCount = 0;

    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    size_t _currentFrame = 0;

    const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };

    std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    std::vector<const char*> instanceExtensions = {};
    virtual void addDeviceExtensions() = 0; // child class adds other extensions that are required

    // initialization functions
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createFramebuffers();
    void createCommandPool();
    void createImageViews();
    
    //void createRenderPass();
    //void createGraphicsPipeline();
    //void createVertexBuffer();
    //void createIndexBuffer();
    //void createCommandBuffers();
    //void createSyncObjects();

    // main loop
    void recreateSwapChain();
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

    // clean up functions
    void cleanupSwapChain();

    // helper functions
    bool checkValidationLayerSupport();
    VkResult createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
    void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
    void testDebugMessenger();
    
    bool isDeviceSuitable(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    std::vector<const char*> getRequiredExtensions();
    
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    static std::vector<char> readFile(const std::string filename);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    VkPipelineShaderStageCreateInfo loadShader(const std::string filename, VkShaderStageFlagBits stage);

    void createBuffer(Buffer buffer, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkDeviceSize size);
    void uploadBuffer(Buffer buffer, VkDeviceSize size, void* dataSrc = nullptr);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void recordImageLayoutTransition(VkCommandBuffer& commandBuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
        VkImageSubresourceRange subresourceRange, VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
};

