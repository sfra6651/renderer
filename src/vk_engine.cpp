#include "vk_engine.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>

#include "lib/utils.h"
#include "vk_initializers.h"
#include "vk_images.h"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "vulkan/vulkan_core.h"

constexpr bool useValidationLayers =
#ifdef DEBUG
    true;
#else
    false;
#endif


void VulkanEngine::init() {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    logErr("SDL_Init failed:", SDL_GetError());
    std::abort();
  }

  this->window = SDL_CreateWindow("renderer",
                             static_cast<int>(this->windowExtent.width),
                             static_cast<int>(this->windowExtent.height),
                             SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  if (!this->window) {
    logErr("SDL_CreateWindow failed:", SDL_GetError());
    std::abort();
  }

  init_vulkan();

  init_swapchain();

  init_commands();

  init_sync_structures();

  this->isInitialized = true;
}


void VulkanEngine::init_vulkan() {
  vkb::InstanceBuilder builder;
  auto inst_ret = builder.set_app_name("renderer")
                    .request_validation_layers(useValidationLayers)
                    .use_default_debug_messenger()
                    .require_api_version(1, 3, 0)
  #ifdef __APPLE__
                    .enable_extension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)
                    .set_app_name("renderer")
  #endif
                    .build();
  if (!inst_ret) {
    logErr("vk-bootstrap instance build failed:", inst_ret.error().message());
    std::abort();
  }
  vkb::Instance vkb_inst = inst_ret.value();
  this->instance = vkb_inst.instance;
  this->debugMessenger = vkb_inst.debug_messenger;

  if (!SDL_Vulkan_CreateSurface(this->window, this->instance, nullptr, &this->surface)) {
    logErr("SDL_Vulkan_CreateSurface failed:", SDL_GetError());
    std::abort();
  }

  VkPhysicalDeviceVulkan13Features features13{};
  features13.dynamicRendering = VK_TRUE;
  features13.synchronization2 = VK_TRUE;

  VkPhysicalDeviceVulkan12Features features12{};
  features12.bufferDeviceAddress = VK_TRUE;
  features12.descriptorIndexing = VK_TRUE;

  vkb::PhysicalDeviceSelector selector{vkb_inst};
  auto physicalDeviceR = selector.set_minimum_version(1, 3)
                      .set_required_features_13(features13)
                      .set_required_features_12(features12)
                      .set_surface(this->surface)
                      .select();
    if (!physicalDeviceR.has_value()) {
    logErr("vk-bootstrap GPU selection failed:", physicalDeviceR.error().message());
    logErr("If on macOS, verify MoltenVK supports Vulkan 1.3 with dynamic rendering + sync2.");
    std::abort();
  }
  vkb::PhysicalDevice physicalDevice = physicalDeviceR.value();

  vkb::DeviceBuilder device_builder{physicalDevice};
  auto vkb_deviceR = device_builder.build();
  if (!vkb_deviceR.has_value()) {
    logErr("vk-bootstrap device build failed:", vkb_deviceR.error().message());
    std::abort();
  }
  vkb::Device vkb_device = vkb_deviceR.value();
  this->device = vkb_device.device;
  this->chosenGpu = physicalDevice.physical_device;

  log("Vulkan initialized:", physicalDevice.name);

  this->graphicsQueue = vkb_device.get_queue(vkb::QueueType::graphics).value();
  this->graphicsQueueFamily = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

  // initialize the memory allocator
  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.physicalDevice = this->chosenGpu;
  allocatorInfo.device = this->device;
  allocatorInfo.instance = this->instance;
  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  vmaCreateAllocator(&allocatorInfo, &this->allocator);

  this->mainDeletionQueue.push_function([&]() {
      vmaDestroyAllocator(this->allocator);
  });
}


void VulkanEngine::create_swapchain(uint32_t width, uint32_t height) {
  vkb::SwapchainBuilder swapchainBuilder { this->chosenGpu, this->device, this->surface };

  this->swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

  vkb::Swapchain vkbSwapchain = swapchainBuilder
    .set_desired_format(VkSurfaceFormatKHR { .format = this->swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
    .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
    .set_desired_extent(width, height)
    .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    .build()
    .value();

  this->swapchain = vkbSwapchain.swapchain;
  this->swapchainExtent = vkbSwapchain.extent;
  this->swapchainImages = vkbSwapchain.get_images().value();
  this->swapchainImageViews = vkbSwapchain.get_image_views().value();
}


void VulkanEngine::destroy_swapchain() {
  vkDestroySwapchainKHR(this->device, this->swapchain, nullptr);

  for (int i = 0; i < this->swapchainImageViews.size(); i++) {
    vkDestroyImageView(this->device, this->swapchainImageViews[i], nullptr);
  }
}


void VulkanEngine::init_swapchain() {
  create_swapchain(this->windowExtent.width, this->windowExtent.height);
  	//draw image size will match the window
	VkExtent3D drawImageExtent = {
		this->windowExtent.width,
		this->windowExtent.height,
		1
	};

	//hardcoding the draw format to 32 bit float
	this->drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	this->drawImage.imageExtent = drawImageExtent;

	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VkImageCreateInfo drawImageInfo = vkinit::image_create_info(this->drawImage.imageFormat, drawImageUsages, drawImageExtent);

	//for the draw image, we want to allocate it from gpu local memory
	VmaAllocationCreateInfo drawImageAllocInfo = {};
	drawImageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	drawImageAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//allocate and create the image
	vmaCreateImage(this->allocator, &drawImageInfo, &drawImageAllocInfo, &this->drawImage.image, &this->drawImage.allocation, nullptr);

	//build a image-view for the draw image to use for rendering
	VkImageViewCreateInfo drawImageViewInfo = vkinit::imageview_create_info(this->drawImage.imageFormat, this->drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

	VK_CHECK(vkCreateImageView(this->device, &drawImageViewInfo, nullptr, &this->drawImage.imageView));

	//add to deletion queues
	this->mainDeletionQueue.push_function([&]() {
		vkDestroyImageView(this->device, this->drawImage.imageView, nullptr);
		vmaDestroyImage(this->allocator, this->drawImage.image, this->drawImage.allocation);
	});
};


void VulkanEngine::init_commands() {
  //create a command pool for commands submitted to the graphics queue.
	//we also want the pool to allow for resetting of individual command buffers
	VkCommandPoolCreateInfo commandPoolInfo =  vkinit::command_pool_create_info(this->graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	
	for (int i = 0; i < FRAME_OVERLAP; i++) {

		VK_CHECK(vkCreateCommandPool(this->device, &commandPoolInfo, nullptr, &this->frames[i].commandPool));

		// allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(this->frames[i].commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(this->device, &cmdAllocInfo, &this->frames[i].mainCommandBuffer));
	}
};


void VulkanEngine::init_sync_structures() {
	//one fence to control when the gpu has finished rendering the frame,
	//and 2 semaphores to syncronize rendering with swapchain
	//we want the fence to start signalled so we can wait on it on the first frame
	VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		VK_CHECK(vkCreateFence(this->device, &fenceCreateInfo, nullptr, &this->frames[i].renderFence));

		VK_CHECK(vkCreateSemaphore(this->device, &semaphoreCreateInfo, nullptr, &this->frames[i].presentSemaphore));
	}

  this->renderSemaphores.resize(swapchainImages.size());
  for (int i = 0; i < this->swapchainImages.size(); i++) {
		VK_CHECK(vkCreateSemaphore(this->device, &semaphoreCreateInfo, nullptr, &this->renderSemaphores[i]));
    this->mainDeletionQueue.push_function([&]() {} );
  }
};


void VulkanEngine::draw() {
  // wait until the gpu has finished rendering the last frame. Timeout of 1second
	VK_CHECK(vkWaitForFences(this->device, 1, &get_current_frame().renderFence, true, 1000000000));
	VK_CHECK(vkResetFences(this->device, 1, &get_current_frame().renderFence));

  //request image from the swapchain
	uint32_t swapchainImageIndex;
	VK_CHECK(vkAcquireNextImageKHR(this->device, this->swapchain, 1000000000, get_current_frame().presentSemaphore, nullptr, &swapchainImageIndex));

  VkCommandBuffer cmd = get_current_frame().mainCommandBuffer;

	// now that we are sure that the commands finished executing, we can safely
	// reset the command buffer to begin recording again.
	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	//start the command buffer recording
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  //make the swapchain image into writeable mode before rendering
	vkutil::transition_image(cmd, this->swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	//make a clear-color from frame number. This will flash with a 120 frame period.
	VkClearColorValue clearValue;
	float flash = std::abs(std::sin(this->frameNumber / 120.f));
	clearValue = { { 0.0f, 0.0f, flash, 1.0f } };

	VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

	//clear image
	vkCmdClearColorImage(cmd, this->swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

	//make the swapchain image into presentable mode
	vkutil::transition_image(cmd, this->swapchainImages[swapchainImageIndex],VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

  //prepare the submission to the queue. 
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished

	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);	
	
	VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
    get_current_frame().presentSemaphore
  );
	VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, this->renderSemaphores[swapchainImageIndex]);	
	
	VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);	

	//submit command buffer to the queue and execute it.
	// renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(this->graphicsQueue, 1, &submit, get_current_frame().renderFence));

  get_current_frame().deletionQueue.flush();

  //prepare present
	// this will put the image we just rendered to into the visible window.
	// we want to wait on the _renderSemaphore for that, 
	// as its necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &this->swapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = &this->renderSemaphores[swapchainImageIndex];
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(this->graphicsQueue, &presentInfo));

	//increase the number of frames drawn
	frameNumber++;
}


void VulkanEngine::run() {
  SDL_Event e;
  bool quit = false;

  Uint64 start = SDL_GetTicks();
  int frameCounter = 0;
  while (!quit) {
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT) quit = true;
      if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) quit = true;
    }
    draw();
    ++frameCounter;
    Uint64 now = SDL_GetTicks();
    if ((now - start) >= 1000) {
      log("frame rate = ", frameCounter); 
      frameCounter = 0;
      start = now;
    }
    this->mainDeletionQueue.flush();
  }
}


void VulkanEngine::cleanup() {
  if (!this->isInitialized) return;
  vkDeviceWaitIdle(this->device);

  for(int i = 0; i < FRAME_OVERLAP; i++) {
    vkDestroyCommandPool(this->device, this->frames[i].commandPool, nullptr);

    //destroy sync objects
		vkDestroyFence(this->device, this->frames[i].renderFence, nullptr);
		vkDestroySemaphore(this->device ,this->frames[i].presentSemaphore, nullptr);
  }
  for (int i = 0; i < this->swapchainImages.size(); i++) {
		vkDestroySemaphore(this->device, this->renderSemaphores[i], nullptr);
  }

  this->mainDeletionQueue.flush();
  destroy_swapchain();
  vkDestroyDevice(this->device, nullptr);
  vkDestroySurfaceKHR(this->instance, this->surface, nullptr);
  vkb::destroy_debug_utils_messenger(this->instance, this->debugMessenger);
  vkDestroyInstance(this->instance, nullptr);
  SDL_DestroyWindow(this->window);
}



