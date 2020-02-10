#include "RendererRTX.h"

RendererRTX::RendererRTX() {
    deviceExtensions.push_back("VK_KHR_swapchain");
    deviceExtensions.push_back("VK_NV_ray_tracing");
}