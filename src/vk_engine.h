#pragma once

#include <camera.h>
#include <vk_descriptors.h>
#include <vk_loader.h>
#include <vk_pipelines.h>
#include <vk_initializers.h>
#include <vk_types.h>

struct SDL_Window;

constexpr uint32_t FRAME_OVERLAP = 2;

struct FrameData {
  VkCommandPool commandPool;
  VkCommandBuffer mainCommandBuffer;

  VkSemaphore presentSemaphore;
  VkFence renderFence;

  DeletionQueue deletionQueue;
  DescriptorAllocator frameDescriptors;
};

class VulkanEngine {
 public:
  Camera mainCamera;
  vkutil::DescriptorAllocator globalDescriptorAllocator;
  VkDescriptorSet drawImageDescriptors;
  VkDescriptorSetLayout drawImageDescriptorLayout;
  VkDescriptorSetLayout gpuSceneDataDescriptorLayout;

	VkPipelineLayout gradientPipelineLayout;
  VkPipeline gradientPipeline;
  VkPipelineLayout meshPipelineLayout;
  VkPipeline meshPipeline;

  std::vector<std::shared_ptr<MeshAsset>> testMeshes;

  //immediate submit structures
  VkFence immediateFence;
  VkCommandBuffer immediateCommandBuffer;
  VkCommandPool immediateCommandPool;

  std::vector<ComputeEffect> backgroundEffects;
  int currentBackgroundEffect {0};

  GPUSceneData sceneData;

  void immediate_submit(std::function<void(VkCommandBuffer cmd)> && function);

  void init();
  void run();
  void cleanup();

  GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

 private:
  void init_vulkan(); //instance, window and devices
  void init_swapchain();
  void init_commands();
  void init_sync_structures();
  void init_descriptors();
  void init_pipelines();
  void init_push_constant_pipelines();
  void init_mesh_pipeline();

  void init_default_data();

  void init_imgui();

  void create_swapchain(uint32_t, uint32_t);
  void resize_swapchain();
  void destroy_swapchain();

  AllocatedBuffer create_buffer(
    size_t allocSize,
    VkBufferUsageFlags usage,
    VmaMemoryUsage memoryUsage,
    VmaAllocationCreateFlags allocFlags
  );
  void destroy_buffer(const AllocatedBuffer& buffer);

  void update_scene();

  void draw();
  void draw_background(VkCommandBuffer);
  void draw_geometry(VkCommandBuffer cmd);
  void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);

  FrameData& get_current_frame() { return frames[frameNumber % FRAME_OVERLAP]; };

  VmaAllocator allocator;

  bool isInitialized = false;
  uint32_t frameNumber = 0;
  FrameData frames[FRAME_OVERLAP];
  VkExtent2D windowExtent {2560, 1440};
  float renderScale = 1.0f;
  SDL_Window* window = nullptr;
  bool resizeRequested = false;

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
  AllocatedImage depthImage;
	VkExtent2D drawExtent;

  VkQueue graphicsQueue;
  uint32_t graphicsQueueFamily;

  std::vector<VkSemaphore> renderSemaphores;

  DeletionQueue mainDeletionQueue;
};
