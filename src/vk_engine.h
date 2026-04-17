#pragma once

#include "vk_types.h"

struct SDL_Window;

class VulkanEngine {
 public:
  void init();
  void run();
  void cleanup();

 private:
  void init_vulkan();

  bool is_initialized_ = false;
  uint32_t frame_number_ = 0;
  VkExtent2D window_extent_{1280, 720};
  SDL_Window* window_ = nullptr;

  VkInstance instance_ = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
  VkPhysicalDevice chosen_gpu_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
};
