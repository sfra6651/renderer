#pragma once

#include "vulkan/vulkan_core.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <span>
#include <vector>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include "lib/utils.h"

#define VK_CHECK(x)                                                            \
  do {                                                                         \
    VkResult err = (x);                                                        \
    if (err) {                                                                 \
      logErr("Detected Vulkan error:", static_cast<int>(err));                 \
      std::abort();                                                            \
    }                                                                          \
  } while (0)

struct ComputePushConstants {
  glm::vec4 data1;
  glm::vec4 data2;
  glm::vec4 data3;
  glm::vec4 data4;
};

struct ComputeEffect {
  const char* name;

  VkPipeline pipeline;
  VkPipelineLayout layout;

  ComputePushConstants data;

};

struct Vk_DeletionQueue 
{
  std::deque<VkImage> images;
  std::deque<VkImageView> imageViews;
  std::deque<VkDescriptorPool> descriptorPools;
  std::deque<VkDescriptorSetLayout> descriptorSetLayouts;
  std::deque<VkPipelineLayout> pipelineLayouts;
  std::deque<VkPipeline> pipelines;


};

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)(); //call functors
		}

		deletors.clear();
	}
};

struct FrameData {
  VkCommandPool commandPool;
  VkCommandBuffer mainCommandBuffer;

  VkSemaphore presentSemaphore;
  VkFence renderFence;

  DeletionQueue deletionQueue;
};

struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

