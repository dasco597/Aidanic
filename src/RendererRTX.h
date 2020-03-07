#pragma once

#include "RendererBase.h"

class RendererRTX : public RendererBase {
public:
	void Init(IOInterface* ioInterface);
	void DrawFrame(bool framebufferResized);
	void CleanUp();

private:

	struct StorageImage {
		VkDeviceMemory memory;
		VkImage image;
		VkImageView view;
		VkFormat format;
	};

	struct AccelerationStructure {
		VkDeviceMemory memory;
		VkAccelerationStructureNV accelerationStructure;
		uint64_t handle;
	};

	// an instance of a bottom level acceleration structure which contains a set of geometry
	struct BLASInstance {
		glm::mat3x4 transform;
		uint32_t instanceId : 24;
		uint32_t mask : 8;
		uint32_t instanceOffset : 24;
		uint32_t flags : 8;
		uint64_t accelerationStructureHandle;
	};

	VkPhysicalDeviceRayTracingPropertiesNV rayTracingProperties{};
	StorageImage renderImage;
	AccelerationStructure bottomLevelAS, topLevelAS;
	Buffer shaderBindingTable;

	struct {
		glm::mat4 viewInverse = glm::mat4(1.0f);
		glm::mat4 projInverse = glm::mat4(1.0f);
	} uniformData;
	Buffer ubo;

	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;

	VkDescriptorSet descriptorSet;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;

	std::vector<VkCommandBuffer> commandBuffersDraw;

    void addDeviceExtensions();

    void createRenderImage(); // create the storage image the ray gen shader will be writing to
	void createScene(); // create scene geometry and acceleration structures
	void createUniformBuffer();
	void createRayTracingPipeline();
	void createShaderBindingTable();
	void createDescriptorSets();
	void buildCommandBuffers();
	
	void createBottomLevelAccelerationStructure(const VkGeometryNV* geometries); // bottom level AS contains scene geometry
	void createTopLevelAccelerationStructure(); // top level AS contains the scene's object (BLAS) instances

	// function pointers for VK_NV_ray_tracing
	PFN_vkCreateAccelerationStructureNV vkCreateAccelerationStructureNV;
	PFN_vkDestroyAccelerationStructureNV vkDestroyAccelerationStructureNV;
	PFN_vkBindAccelerationStructureMemoryNV vkBindAccelerationStructureMemoryNV;
	PFN_vkGetAccelerationStructureHandleNV vkGetAccelerationStructureHandleNV;
	PFN_vkGetAccelerationStructureMemoryRequirementsNV vkGetAccelerationStructureMemoryRequirementsNV;
	PFN_vkCmdBuildAccelerationStructureNV vkCmdBuildAccelerationStructureNV;
	PFN_vkCreateRayTracingPipelinesNV vkCreateRayTracingPipelinesNV;
	PFN_vkGetRayTracingShaderGroupHandlesNV vkGetRayTracingShaderGroupHandlesNV;
	PFN_vkCmdTraceRaysNV vkCmdTraceRaysNV;
};