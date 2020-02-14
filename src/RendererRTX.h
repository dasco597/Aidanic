#pragma once

#include "RendererBase.h"

class RendererRTX : public RendererBase {
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

	StorageImage renderImage;
	AccelerationStructure bottomLevelAS, topLevelAS;

	// todo init
	// todo clean-up
    void addDeviceExtensions();

    void createRenderImage(); // create the storage image the ray gen shader will be writing to
	void createScene(); // create scene geometry and acceleration structures
	void createBottomLevelAccelerationStructure(const VkGeometryNV* geometries); // bottom level AS contains scene geometry
	void createTopLevelAccelerationStructure(); // top level AS contains the scene's object (BLAS) instances
};