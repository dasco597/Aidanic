#include "ImguiVk.h"
#include "Renderer.h"
#include <imgui.h>

#define SHADER_SRC_IMGUI_VERT "spirv/imgui.vert.spv"
#define SHADER_SRC_IMGUI_FRAG "spirv/imgui.frag.spv"

void ImGuiVk::init() {
    perFrameResources.resize(Renderer::getNumSwapchainImages());
    
    createFontTexture();
    createDescriptorSets();
    createRenderPass();
    createFramebuffer();
    createPipeline();
    createCommandBuffers();
}

void ImGuiVk::createFramebuffer() {
    VkImageView attachments[] = {
        Renderer::getRenderImage().view
    };

    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderpass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = Renderer::getRenderImage().extent.width;
    framebufferInfo.height = Renderer::getRenderImage().extent.height;
    framebufferInfo.layers = 1;

    VK_CHECK_RESULT(vkCreateFramebuffer(Renderer::getDevice(), &framebufferInfo, nullptr, &framebuffer), "failed to create imgui framebuffer");

    // render image barrier

    VkImageSubresourceRange imageSubresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    renderImageBarrierGeneral.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    renderImageBarrierGeneral.pNext = VK_NULL_HANDLE;
    renderImageBarrierGeneral.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    renderImageBarrierGeneral.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    renderImageBarrierGeneral.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    renderImageBarrierGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    renderImageBarrierGeneral.srcQueueFamilyIndex = 0;
    renderImageBarrierGeneral.dstQueueFamilyIndex = 0;
    renderImageBarrierGeneral.image = Renderer::getRenderImage().image;
    renderImageBarrierGeneral.subresourceRange = imageSubresourceRange;
}

void ImGuiVk::createFontTexture() {
    ImGuiIO& io = ImGui::GetIO();

    // get font data

    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    size_t uploadSize = width * height * 4 * sizeof(char);
    fontTexture.extent = VkExtent2D{ static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

    // create image
    {
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VK_CHECK_RESULT(vkCreateImage(Renderer::getDevice(), &imageInfo, VK_ALLOCATOR, &fontTexture.image), "failed to create imgui font image");

        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(Renderer::getDevice(), fontTexture.image, &req);
        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = req.size;
        allocInfo.memoryTypeIndex = Vk::findMemoryType(Renderer::getPhysicalDevice(), req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(Renderer::getDevice(), &allocInfo, VK_ALLOCATOR, &fontTexture.memory), "failed to allocate imgui font texture memory");
        VK_CHECK_RESULT(vkBindImageMemory(Renderer::getDevice(), fontTexture.image, fontTexture.memory, 0), "failed to bind imgui font image memory");
    }

    // create image view
    {
        VkImageViewCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = fontTexture.image;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = VK_FORMAT_R8G8B8A8_UNORM;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 1;
        VK_CHECK_RESULT(vkCreateImageView(Renderer::getDevice(), &info, VK_ALLOCATOR, &fontTexture.view), "failed to create imgui font imageview");
    }

    // upload pixel data
    {
        Vk::BufferHostVisible uploadBuffer;
        uploadBuffer.create(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, uploadSize, Renderer::getDevice(), Renderer::getPhysicalDevice());
        uploadBuffer.upload(static_cast<void*>(pixels), uploadSize, 0, Renderer::getDevice());

        VkImageMemoryBarrier copyBarrier[1] = {};
        copyBarrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        copyBarrier[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        copyBarrier[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        copyBarrier[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copyBarrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copyBarrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copyBarrier[0].image = fontTexture.image;
        copyBarrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyBarrier[0].subresourceRange.levelCount = 1;
        copyBarrier[0].subresourceRange.layerCount = 1;

        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width = width;
        region.imageExtent.height = height;
        region.imageExtent.depth = 1;

        VkImageMemoryBarrier useBarrier[1] = {};
        useBarrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        useBarrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        useBarrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        useBarrier[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        useBarrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        useBarrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        useBarrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        useBarrier[0].image = fontTexture.image;
        useBarrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        useBarrier[0].subresourceRange.levelCount = 1;
        useBarrier[0].subresourceRange.layerCount = 1;

        VkCommandBuffer commandBuffer = Vk::beginSingleTimeCommands(Renderer::getDevice(), Renderer::getCommandPool());

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, copyBarrier);
        vkCmdCopyBufferToImage(commandBuffer, uploadBuffer.buffer, fontTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, useBarrier);

        Vk::endSingleTimeCommands(Renderer::getDevice(), commandBuffer, Renderer::getGraphicsQueue(), Renderer::getCommandPool());

        uploadBuffer.destroy(Renderer::getDevice());
    }

    // store identifier

    io.Fonts->TexID = (ImTextureID)(intptr_t)fontTexture.image;

    // create sampler
    {
        VkSamplerCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter = VK_FILTER_LINEAR;
        info.minFilter = VK_FILTER_LINEAR;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.minLod = -1000;
        info.maxLod = 1000;
        info.maxAnisotropy = 1.0f;
        vkCreateSampler(Renderer::getDevice(), &info, VK_ALLOCATOR, &fontSampler);
    }
}

void ImGuiVk::createDescriptorSets() {
    // create descriptor set pool
    {
        std::vector<VkDescriptorPoolSize> poolSizes = {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        };

        VkDescriptorPoolCreateInfo descriptorPoolCI{};
        descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        descriptorPoolCI.pPoolSizes = poolSizes.data();
        descriptorPoolCI.maxSets = 1;
        VK_CHECK_RESULT(vkCreateDescriptorPool(Renderer::getDevice(), &descriptorPoolCI, VK_ALLOCATOR, &descriptorPool), "failed to create descriptor pool");
    }

    // create descriptor set layout
    {
        VkSampler sampler[1] = { fontSampler };
        VkDescriptorSetLayoutBinding binding[1] = {};
        binding[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding[0].descriptorCount = 1;
        binding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        binding[0].pImmutableSamplers = sampler;
        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = 1;
        info.pBindings = binding;
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(Renderer::getDevice(), &info, VK_ALLOCATOR, &descriptorSetLayout), "failed to create imgui descriptor set layout");
    }

    // create descriptor set
    {
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = descriptorPool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &descriptorSetLayout;
        VK_CHECK_RESULT(vkAllocateDescriptorSets(Renderer::getDevice(), &alloc_info, &descriptorSet), "failed to allocate imgui descriptor set");
    }

    // update descriptor set
    {
        VkDescriptorImageInfo desc_image[1] = {};
        desc_image[0].sampler = fontSampler;
        desc_image[0].imageView = fontTexture.view;
        desc_image[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet write_desc[1] = {};
        write_desc[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_desc[0].dstSet = descriptorSet;
        write_desc[0].descriptorCount = 1;
        write_desc[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write_desc[0].pImageInfo = desc_image;
        vkUpdateDescriptorSets(Renderer::getDevice(), 1, write_desc, 0, NULL);
    }
}

void ImGuiVk::createRenderPass() {
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = Renderer::getRenderImage().format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VK_CHECK_RESULT(vkCreateRenderPass(Renderer::getDevice(), &renderPassInfo, nullptr, &renderpass), "failed to create render pass");
}

void ImGuiVk::createPipeline() {

    // pipeline layout

    VkPushConstantRange pushConstantRanges[1];
    pushConstantRanges[0].offset = 0;
    pushConstantRanges[0].size = sizeof(float) * 4;
    pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges;

    VK_CHECK_RESULT(vkCreatePipelineLayout(Renderer::getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout), "failed to create imgui pipeline layout");

    // pipeline

    auto vertShaderCode = Vk::readFile(_CONFIG::getAssetsPath() + std::string(SHADER_SRC_IMGUI_VERT));
    auto fragShaderCode = Vk::readFile(_CONFIG::getAssetsPath() + std::string(SHADER_SRC_IMGUI_FRAG));

    VkShaderModule vertShaderModule = Vk::createShaderModule(Renderer::getDevice(), vertShaderCode);
    VkShaderModule fragShaderModule = Vk::createShaderModule(Renderer::getDevice(), fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(ImDrawVert);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(ImDrawVert, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(ImDrawVert, uv);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    attributeDescriptions[2].offset = offsetof(ImDrawVert, col);

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineDepthStencilStateCreateInfo depthInfo = {};
    depthInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkDynamicState dynamicStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthInfo;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderpass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(Renderer::getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline), "failed to create imgui pipeline");

    vkDestroyShaderModule(Renderer::getDevice(), fragShaderModule, nullptr);
    vkDestroyShaderModule(Renderer::getDevice(), vertShaderModule, nullptr);
}

void ImGuiVk::createCommandBuffers() {
    // command pool

    Vk::QueueFamilyIndices queueFamilyIndices = Vk::findQueueFamilies(Renderer::getPhysicalDevice(), Renderer::getSurface());

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    VK_CHECK_RESULT(vkCreateCommandPool(Renderer::getDevice(), &poolInfo, nullptr, &commandPool), "failed to create graphics command pool!");

    // command buffers

    VkCommandBufferAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = commandPool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;
    for (int i = 0; i < Renderer::getNumSwapchainImages(); i++)
        VK_CHECK_RESULT(vkAllocateCommandBuffers(Renderer::getDevice(), &info, &perFrameResources[i].commandBuffer), "failed to allocate imgui command buffer");

}

void ImGuiVk::recordRenderCommands(uint32_t swapchainIndex) {
    ImDrawData* draw_data = ImGui::GetDrawData();
    PerFrame& frameResources = perFrameResources[swapchainIndex];

    int fb_width = (int)(draw_data->DisplaySize.x);
    int fb_height = (int)(draw_data->DisplaySize.y);
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    if (fb_width <= 0 || fb_height <= 0 || draw_data->TotalVtxCount == 0) {
        frameResources.render = false;
        return;
    }

    // Create or resize the vertex/index buffers
    size_t vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
    size_t index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
    if (vertex_size == 0 || index_size == 0) {
        AID_WARN("no imgui vertices or indices to draw!");
        return;
    }

    if (frameResources.vertexBuffer.buffer == VK_NULL_HANDLE || frameResources.vertexBuffer.size < vertex_size) {
        frameResources.vertexBuffer.destroy(Renderer::getDevice());
        frameResources.vertexBuffer.create(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertex_size, Renderer::getDevice(), Renderer::getPhysicalDevice());
    }
    if (frameResources.indexBuffer.buffer == VK_NULL_HANDLE || frameResources.indexBuffer.size < index_size) {
        frameResources.indexBuffer.destroy(Renderer::getDevice());
        frameResources.indexBuffer.create(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, index_size, Renderer::getDevice(), Renderer::getPhysicalDevice());
    }

    // Upload vertex/index data
    {
        ImDrawVert* vtx_dst;
        ImDrawIdx* idx_dst;
        VK_CHECK_RESULT(vkMapMemory(Renderer::getDevice(), frameResources.vertexBuffer.memory, 0, vertex_size, 0, (void**)(&vtx_dst)), "failed to map imgui vertex buffer memory");
        VK_CHECK_RESULT(vkMapMemory(Renderer::getDevice(), frameResources.indexBuffer.memory, 0, index_size, 0, (void**)(&idx_dst)), "failed to map imgui index buffer memory");

        for (int n = 0; n < draw_data->CmdListsCount; n++) {
            const ImDrawList* cmd_list = draw_data->CmdLists[n];
            memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtx_dst += cmd_list->VtxBuffer.Size;
            idx_dst += cmd_list->IdxBuffer.Size;
        }
        VkMappedMemoryRange range[2] = {};
        range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range[0].memory = frameResources.vertexBuffer.memory;
        range[0].size = VK_WHOLE_SIZE;
        range[1].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range[1].memory = frameResources.indexBuffer.memory;
        range[1].size = VK_WHOLE_SIZE;
        VK_CHECK_RESULT(vkFlushMappedMemoryRanges(Renderer::getDevice(), 2, range), "failed to flush imgui vertex and index memory");

        vkUnmapMemory(Renderer::getDevice(), frameResources.vertexBuffer.memory);
        vkUnmapMemory(Renderer::getDevice(), frameResources.indexBuffer.memory);
    }

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Record draw commands
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkCommandBuffer commandBuffer = frameResources.commandBuffer;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    setupRenderState(commandBuffer, frameResources, fb_width, fb_height, draw_data);

    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != NULL) {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    setupRenderState(commandBuffer, frameResources, fb_width, fb_height, draw_data);
                else
                    pcmd->UserCallback(cmd_list, pcmd);
            } else {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec4 clip_rect;
                clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
                clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
                clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
                clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

                if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f) {
                    // Negative offsets are illegal for vkCmdSetScissor
                    if (clip_rect.x < 0.0f)
                        clip_rect.x = 0.0f;
                    if (clip_rect.y < 0.0f)
                        clip_rect.y = 0.0f;

                    // Apply scissor/clipping rectangle
                    VkRect2D scissor;
                    scissor.offset.x = (int32_t)(clip_rect.x);
                    scissor.offset.y = (int32_t)(clip_rect.y);
                    scissor.extent.width = (uint32_t)(clip_rect.z - clip_rect.x);
                    scissor.extent.height = (uint32_t)(clip_rect.w - clip_rect.y);
                    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

                    // Draw
                    vkCmdDrawIndexed(commandBuffer, pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
                }
            }
        }
        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }

    vkCmdEndRenderPass(commandBuffer);
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &renderImageBarrierGeneral);
    vkEndCommandBuffer(commandBuffer);
    frameResources.render = true;
}

void ImGuiVk::recreateFramebuffer() {
    vkDestroyFramebuffer(Renderer::getDevice(), framebuffer, VK_ALLOCATOR);
    createFramebuffer();
}

void ImGuiVk::setupRenderState(VkCommandBuffer commandBuffer, PerFrame& perFrameResources, int fb_width, int fb_height, ImDrawData* draw_data) {
    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = renderpass;
    renderPassBeginInfo.framebuffer = framebuffer;
    renderPassBeginInfo.renderArea.extent = Renderer::getRenderImage().extent;
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = &clearValue;

    VkViewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)fb_width;
    viewport.height = (float)fb_height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    float scale[2];
    scale[0] = 2.0f / draw_data->DisplaySize.x;
    scale[1] = 2.0f / draw_data->DisplaySize.y;
    float translate[2];
    translate[0] = -1.0f - draw_data->DisplayPos.x * scale[0];
    translate[1] = -1.0f - draw_data->DisplayPos.y * scale[1];

    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &perFrameResources.vertexBuffer.buffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, perFrameResources.indexBuffer.buffer, 0, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 0, sizeof(float) * 2, scale);
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 2, sizeof(float) * 2, translate);
}

void ImGuiVk::cleanup() {
    vkDeviceWaitIdle(Renderer::getDevice());

    vkDestroyPipeline(Renderer::getDevice(), pipeline, VK_ALLOCATOR);
    vkDestroyPipelineLayout(Renderer::getDevice(), pipelineLayout, VK_ALLOCATOR);

    vkDestroyRenderPass(Renderer::getDevice(), renderpass, VK_ALLOCATOR);
    vkDestroyFramebuffer(Renderer::getDevice(), framebuffer, VK_ALLOCATOR);

    fontTexture.destroy(Renderer::getDevice());
    vkDestroySampler(Renderer::getDevice(), fontSampler, VK_ALLOCATOR);

    for (int f = 0; f < perFrameResources.size(); f++) {
        perFrameResources[f].vertexBuffer.destroy(Renderer::getDevice());
        perFrameResources[f].indexBuffer.destroy(Renderer::getDevice());
    }

    vkDestroyDescriptorSetLayout(Renderer::getDevice(), descriptorSetLayout, VK_ALLOCATOR);
    vkDestroyDescriptorPool(Renderer::getDevice(), descriptorPool, VK_ALLOCATOR);

    vkDestroyCommandPool(Renderer::getDevice(), commandPool, VK_ALLOCATOR);
}
