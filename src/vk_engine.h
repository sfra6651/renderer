#pragma once

#include "vk_descriptors.h"
#include "vk_pipelines.h"
#include "vk_types.h"

struct SDL_Window;

constexpr uint32_t FRAME_OVERLAP = 2;

class VulkanEngine {
 public:
  vkutil::DescriptorAllocator globalDescriptorAllocator;
  VkDescriptorSet drawImageDescriptors;
  VkDescriptorSetLayout drawImageDescriptorLayout;
  VkPipeline gradientPipeline;
	VkPipelineLayout gradientPipelineLayout;

  //immediate submit structures
  VkFence imGuiFence;
  VkCommandBuffer imGuiCommandBuffer;
  VkCommandPool imGuiCommandPool;

  void immediate_submit(std::function<void(VkCommandBuffer cmd)> && function);

  void init();
  void run();
  void cleanup();

 private:
  void init_vulkan(); //instance, window and devices
  void init_swapchain();
  void init_commands();
  void init_sync_structures();
  void init_descriptors();
  void init_pipelines();
  void init_background_pipelines();

  void init_imgui();

  void draw();
  void draw_background(VkCommandBuffer);

  void create_swapchain(uint32_t, uint32_t);
  void destroy_swapchain();


  FrameData& get_current_frame() { return frames[frameNumber % FRAME_OVERLAP]; };

  VmaAllocator allocator;

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

  //draw rescources
  AllocatedImage drawImage;
	VkExtent2D drawExtent;

  VkQueue graphicsQueue;
  uint32_t graphicsQueueFamily;

  std::vector<VkSemaphore> renderSemaphores;

  DeletionQueue mainDeletionQueue;
};
