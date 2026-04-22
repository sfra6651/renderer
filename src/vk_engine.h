#pragma once

#include "vk_types.h"

struct SDL_Window;

struct FrameData {
  VkCommandPool commandPool;
  VkCommandBuffer mainCommandBuffer;

  VkSemaphore swapchainSemaphore, renderSemaphore;
  VkFence renderFence;
};

constexpr uint32_t FRAME_OVERLAP = 2;

class VulkanEngine {
 public:
  void init();
  void run();
  void cleanup();

 private:
  void init_vulkan(); //instance, window and devices
  void init_swapchain();
  void init_commands();
  void init_sync_structures();
  void draw();

  void create_swapchain(uint32_t, uint32_t);
  void destroy_swapchain();


  FrameData& get_current_frame() { return frames[frameNumber % FRAME_OVERLAP]; };

  bool isInitialized = false;
  uint32_t frameNumber = 0;
  FrameData frames[FRAME_OVERLAP];
  VkExtent2D windowExtent {1280, 720};
  SDL_Window* window = nullptr;

  VkInstance instance;
  VkDebugUtilsMessengerEXT debugMessenger;
  VkPhysicalDevice chosenGpu;
  VkDevice device;
  VkSurfaceKHR surface;

  VkSwapchainKHR swapchain;
  VkFormat swapchainImageFormat;

  std::vector<VkImage> swapchainImages;
  std::vector<VkImageView> swapchainImageViews;
  VkExtent2D swapchainExtent;

  VkQueue graphicsQueue;
  uint32_t graphicsQueueFamily;
};
