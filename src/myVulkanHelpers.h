#pragma once

#include "vulkan/vulkan_structs.hpp"
#define GLFW_INCLUDE_VULKAN
#include<GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>
#include "lib/utils.h"

// debug
const std::vector<const char*> validationLayers = {
  "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> requiredExtensions = {
  vk::KHRSwapchainExtensionName
};


struct SwapChainSupportDetails {
  vk::SurfaceCapabilitiesKHR capabilities {};
  std::vector<vk::SurfaceFormatKHR> availableFormats {};
  std::vector<vk::PresentModeKHR> availablePresentModes {};
};


inline bool checkValidationLayerSupport() {
  uint32_t layerCount;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  for (const char* layerName : validationLayers) {
    bool found = false;
    for (const auto& layerProperties : availableLayers) {
      if (strcmp(layerName, layerProperties.layerName) == 0) {
        found = true;
        break;
      }
    }

    if (!found) {
      return false;
    }
  }

  return true;
}

inline bool checkDeviceExtensionSupport(const vk::raii::PhysicalDevice& device, const std::vector<const char*>& requiredExtensions) {
  auto deviceExtensions = device.enumerateDeviceExtensionProperties();
  for (const char* requiredExtension : requiredExtensions) {
    bool found = false;
    for (const auto& ext : deviceExtensions) {
      if (strcmp(ext.extensionName, requiredExtension) == 0) {
        found = true;
        break;
      }
    }
    if (!found) return false;
  }

  return true;
}


inline SwapChainSupportDetails querySwapChainSupport(const vk::raii::PhysicalDevice& device, const vk::raii::SurfaceKHR& surface) {
  SwapChainSupportDetails details;
  details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
  details.availableFormats = device.getSurfaceFormatsKHR(surface);
  details.availablePresentModes = device.getSurfacePresentModesKHR(surface);

  return details;
};


inline bool isDeviceSuitable(const vk::raii::PhysicalDevice& device, const vk::raii::SurfaceKHR& surface) {
  bool supportsVulkan1_3 = device.getProperties().apiVersion >= vk::ApiVersion13;

  bool extensionSupported = checkDeviceExtensionSupport(device, requiredExtensions);
  // QueueFamilyIndices indices = findQueueFamilies(device);

  // require a queue family that has both present and graphics support.
  // in practice its rare to find a case where there is at least one family without both
  bool suitableQueueFamily = false;
  auto queueFamilies = device.getQueueFamilyProperties();
  for (uint32_t i = 0; i < queueFamilies.size(); i++) {
    if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics &&
      device.getSurfaceSupportKHR(i, surface) == VK_TRUE) {
      suitableQueueFamily = true;
    }
  }


  auto features = device.getFeatures2<
   vk::PhysicalDeviceFeatures2,
   vk::PhysicalDeviceShaderDrawParametersFeatures, // vk:PhysicalDeviceFeatures11 doesnt behave here for some reason, this is the specific feature we want from there
   vk::PhysicalDeviceVulkan13Features,
   vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
  bool supportsRequiredFeatures =
    features.get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
    features.get<vk::PhysicalDeviceShaderDrawParametersFeatures>().shaderDrawParameters &&
    features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
    features.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
    features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

  bool swapChainAdequate = false;
  if(extensionSupported) {
    SwapChainSupportDetails details = querySwapChainSupport(device, surface);
    swapChainAdequate = !details.availableFormats.empty() && !details.availablePresentModes.empty();
  }

  return supportsVulkan1_3 && suitableQueueFamily && extensionSupported && supportsRequiredFeatures && swapChainAdequate;
}


inline uint64_t rateDevice(const vk::raii::PhysicalDevice& device, const vk::raii::SurfaceKHR& surface) {
  auto deviceProperties = device.getProperties();
  auto deviceFeatures = device.getFeatures();

  uint64_t score = 0;

  if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
    score += 1000;
  }
  score += deviceProperties.limits.maxImageDimension2D;

  if (!isDeviceSuitable(device, surface)) {
    return 0;
  }

#if defined (__APPLE__)
  return score;
#endif

  if (!deviceFeatures.geometryShader) {
    return 0;
  }

  return score;
}


VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
  for (const auto& format : availableFormats) {
    if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
      return format;
    }
  }

  // we could rank the others if what we want isnt found
  // but for now just return the first available
  return availableFormats[0];
}

vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
  for (const auto& mode : availablePresentModes) {
    if (mode == vk::PresentModeKHR::eMailbox) {
      return mode;
    }
  }
  return vk::PresentModeKHR::eFifo;
}

vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) {
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  }

  int width, height;
  glfwGetFramebufferSize(window, &width, &height);

  vk::Extent2D actualExtent = {
    static_cast<uint32_t>(width),
    static_cast<uint32_t>(height),
  };

  actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
  actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

  return actualExtent;
}

uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities)
{
  auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
  if (0 < surfaceCapabilities.maxImageCount && surfaceCapabilities.maxImageCount < minImageCount)
  {
    minImageCount = surfaceCapabilities.maxImageCount;
  }
  return minImageCount;
}

[[nodiscard]] inline vk::raii::ShaderModule createShaderModule(const vk::raii::Device& logicalDevice, const std::vector<char>& code) {
  vk::ShaderModuleCreateInfo createInfo {
    .codeSize = code.size() * sizeof(char),
    .pCode = reinterpret_cast<const uint32_t*>(code.data())
  };
  return vk::raii::ShaderModule(logicalDevice, createInfo);
}


uint32_t findMemoryType(const vk::raii::PhysicalDevice& physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
  vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
  {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
    {
      return i;
    }
  }
  throw std::runtime_error("Failed to find suitable memory type!");
}

