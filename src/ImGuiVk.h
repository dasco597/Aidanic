#pragma once

#include "tools/VkHelper.h"
#include <glm.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

class Renderer;
struct ImGuiIO;
struct ImDrawData;

class ImGuiVk {
public:
    void init(Renderer* renderer);
    float* getpClearValue() { return &clearValue.color.float32[0]; }
    void recordRenderCommands(uint32_t swapchainIndex);
    VkCommandBuffer getCommandBuffer(uint32_t swapchainIndex) { return perFrameResources[swapchainIndex].commandBuffer; }
    bool shouldRender(uint32_t swapchainIndex) { return perFrameResources[swapchainIndex].render; }
    void cleanup();

private:
    Renderer* renderer;

    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    VkRenderPass renderpass;
    VkFramebuffer framebuffer;
    VkClearValue clearValue = { {0.0f, 0.0f, 0.0f, 1.0f} };

    Vk::StorageImage fontTexture;
    VkSampler fontSampler;

    struct VertexData {
        glm::vec2 pos;
        glm::vec2 uv;
        glm::vec4 color;
    };

    VkCommandPool commandPool;
    struct PerFrame {
        Vk::Buffer vertexBuffer, indexBuffer;
        VkCommandBuffer commandBuffer;
        bool render = false;
    };
    VkImageMemoryBarrier renderImageBarrierGeneral;
    std::vector<PerFrame> perFrameResources;

    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;

    void createFramebuffer();
    void createFontTexture();
    void createDescriptorSets();
    void createRenderPass();
    void createPipeline();
    void createCommandBuffers();

    void setupRenderState(VkCommandBuffer& commandBuffer, PerFrame& perFrameResources, int fb_width, int fb_height, ImDrawData* draw_data);
    void recreateBuffer(Vk::Buffer& buffer, VkDeviceSize newSize, VkBufferUsageFlags usage);
};