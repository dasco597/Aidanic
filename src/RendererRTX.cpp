#include "RendererRTX.h"

void RendererRTX::addDeviceExtensions() {
    deviceExtensions.push_back(VK_NV_RAY_TRACING_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME); // required by VK_NV_RAY_TRACING
    instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME); // required by VK_NV_RAY_TRACING
}