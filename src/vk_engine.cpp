#include "vk_engine.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <thread>

#include "lib/utils.h"
#include "vk_initializers.h"
#include "vk_images.h"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "vulkan/vulkan_core.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

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

  init_descriptors();

  init_pipelines();

  init_imgui();

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

  // imgui immediate mode commands
  VK_CHECK(vkCreateCommandPool(this->device, &commandPoolInfo, nullptr, &this->imGuiCommandPool));

	// allocate the command buffer for immediate submits
	VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(this->imGuiCommandPool, 1);

	VK_CHECK(vkAllocateCommandBuffers(this->device, &cmdAllocInfo, &this->imGuiCommandBuffer));

	this->mainDeletionQueue.push_function([&]() { 
	vkDestroyCommandPool(this->device, this->imGuiCommandPool, nullptr);
	});
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

  //imgui fence
  VK_CHECK(vkCreateFence(this->device, &fenceCreateInfo, nullptr, &this->imGuiFence));
	this->mainDeletionQueue.push_function([&]() { vkDestroyFence(this->device, this->imGuiFence, nullptr); });
};

void VulkanEngine::init_descriptors() 
{
  //create a descriptor pool that will hold 10 sets with 1 image each
  std::vector<vkutil::DescriptorAllocator::PoolSizeRatio> sizes = { { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 } };

  this->globalDescriptorAllocator.init_pool(this->device, 10, sizes);

  //make the descriptor set loyout for the compute draw
  {
    vkutil::DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    this->drawImageDescriptorLayout = builder.build(this->device, VK_SHADER_STAGE_COMPUTE_BIT);
  }

  //allocate a sescriptor set for out draw image
  this->drawImageDescriptors = this->globalDescriptorAllocator.allocate(this->device, this->drawImageDescriptorLayout);

  VkDescriptorImageInfo imgInfo {};
  imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  imgInfo.imageView = this->drawImage.imageView;

  VkWriteDescriptorSet drawImageWrite {};
  drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  drawImageWrite.pNext = nullptr;
  drawImageWrite.dstBinding = 0;
  drawImageWrite.dstSet = this->drawImageDescriptors;
  drawImageWrite.descriptorCount = 1;
  drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  drawImageWrite .pImageInfo = &imgInfo;

  vkUpdateDescriptorSets(this->device, 1, &drawImageWrite, 0, nullptr);

  //make sure both the descriptor allocator and the new layout get cleaned up properly
  this->mainDeletionQueue.push_function([&]() {
    this->globalDescriptorAllocator.destroy_pool(this->device);
    vkDestroyDescriptorSetLayout(this->device, this->drawImageDescriptorLayout, nullptr);
  });

}


void VulkanEngine::init_pipelines()
{

  init_push_constant_pipelines();
//  init_background_pipelines();
}

void VulkanEngine::init_push_constant_pipelines()
{
  VkPipelineLayoutCreateInfo computeLayout{};
  computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  computeLayout.pNext = nullptr;
  computeLayout.pSetLayouts = &this->drawImageDescriptorLayout;
  computeLayout.setLayoutCount = 1;

  VkPushConstantRange pushConstant{};
  pushConstant.offset = 0;
  pushConstant.size = sizeof(ComputePushConstants) ;
  pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  computeLayout.pPushConstantRanges = &pushConstant;
  computeLayout.pushConstantRangeCount = 1;

  VK_CHECK(vkCreatePipelineLayout(this->device, &computeLayout, nullptr, &this->gradientPipelineLayout));

  VkShaderModule computeDrawShader;
  if (!vkutil::load_shader_module("bin/shaders/compute_color.spv", this->device, &computeDrawShader))
  {
	  logErr("Error when building the colored mesh shader");
    std::abort();
  }

	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = computeDrawShader;
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = this->gradientPipelineLayout;
	computePipelineCreateInfo.stage = stageinfo;

  ComputeEffect gradient;
  gradient.layout = this->gradientPipelineLayout;
  gradient.name = "gradient";
  gradient.data = {};

  //default colors
  gradient.data.data1 = glm::vec4(1, 0, 0, 1);
  gradient.data.data2 = glm::vec4(0, 0, 1, 1);

  VK_CHECK(vkCreateComputePipelines(
    this->device,
    VK_NULL_HANDLE,
    1,
    &computePipelineCreateInfo,
    nullptr,
    &this->gradientPipeline
  ));

  gradient.pipeline = this->gradientPipeline;
  this->backgroundEffects.push_back(gradient);

  vkDestroyShaderModule(this->device, computeDrawShader, nullptr);

	this->mainDeletionQueue.push_function([&]() {
		vkDestroyPipelineLayout(this->device, this->gradientPipelineLayout, nullptr);
		vkDestroyPipeline(this->device, this->gradientPipeline, nullptr);
		});
}

void VulkanEngine::init_background_pipelines()
{
  VkPipelineLayoutCreateInfo computeLayout{};
	computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext = nullptr;
	computeLayout.pSetLayouts = &this->drawImageDescriptorLayout;
	computeLayout.setLayoutCount = 1;

	VK_CHECK(vkCreatePipelineLayout(this->device, &computeLayout, nullptr, &this->gradientPipelineLayout));

  VkShaderModule computeDrawShader;
	if (!vkutil::load_shader_module("bin/shaders/compute.spv", this->device, &computeDrawShader))
	{
		logErr("Error when building the compute shader");
    std::abort();
	}

	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = computeDrawShader;
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = this->gradientPipelineLayout;
	computePipelineCreateInfo.stage = stageinfo;
	
	VK_CHECK(vkCreateComputePipelines(
    this->device,
    VK_NULL_HANDLE,
    1,
    &computePipelineCreateInfo,
    nullptr,
    &this->gradientPipeline
  ));

  vkDestroyShaderModule(this->device, computeDrawShader, nullptr);

	this->mainDeletionQueue.push_function([&]() {
		vkDestroyPipelineLayout(this->device, this->gradientPipelineLayout, nullptr);
		vkDestroyPipeline(this->device, this->gradientPipeline, nullptr);
		});
}

void VulkanEngine::init_imgui()
{
  // 1: create descriptor pool for IMGUI
	//  the size of the pool is very oversize, but it's copied from imgui demo
	//  itself.
	VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(this->device, &pool_info, nullptr, &imguiPool));

	// 2: initialize imgui library

	// this initializes the core structures of imgui
	ImGui::CreateContext();

	// this initializes imgui for SDL
	ImGui_ImplSDL3_InitForVulkan(this->window);

	// this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = this->instance;
	init_info.PhysicalDevice = this->chosenGpu;
	init_info.Device = this->device;
	init_info.Queue = this->graphicsQueue;
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.UseDynamicRendering = true;

	//dynamic rendering parameters for imgui to use
	init_info.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
	init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &this->swapchainImageFormat;
	

	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info);

	ImGui_ImplVulkan_CreateFontsTexture();

	// add the destroy the imgui created structures
	this->mainDeletionQueue.push_function([this, imguiPool]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(this->device, imguiPool, nullptr);
	});
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)> && function)
{
  VK_CHECK(vkResetFences(this->device, 1, &this->imGuiFence));
  VK_CHECK(vkResetCommandBuffer(this->imGuiCommandBuffer, 0));

  VkCommandBuffer cmd = this->imGuiCommandBuffer;

  VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  function(cmd);

  VK_CHECK(vkEndCommandBuffer(cmd));

  VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);
  VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, nullptr, nullptr);

  //submit command buffer to the queue and execute it
  //this->renderFence will now block until the graphic command finish executing
  VK_CHECK(vkQueueSubmit2(this->graphicsQueue, 1, &submit, this->imGuiFence));
  VK_CHECK(vkWaitForFences(this->device, 1, &this->imGuiFence, true, 9999999999));
}

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

  this->drawExtent.width = this->drawImage.imageExtent.width;
  this->drawExtent.height = this->drawImage.imageExtent.height;

	//start the command buffer recording
		//start the command buffer recording
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));	

	// transition our main draw image into general layout so we can write into it
	// we will overwrite it all so we dont care about what was the older layout
	vkutil::transition_image(cmd, this->drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	draw_background(cmd);

	//transition the draw image and the swapchain image into their correct transfer layouts
	vkutil::transition_image(cmd, this->drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkutil::transition_image(cmd, this->swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// execute a copy from the draw image into the swapchain
	vkutil::copy_image_to_image(cmd, this->drawImage.image, this->swapchainImages[swapchainImageIndex], this->drawExtent, this->swapchainExtent);

  // set the swapchain image layout to Attachemnt Optimal so we can draw it
  vkutil::transition_image(cmd, this->swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  //draw imgui into the swapchain image
  draw_imgui(cmd, this->swapchainImageViews[swapchainImageIndex]);

	// set swapchain image layout to Present so we can show it on the screen
	vkutil::transition_image(cmd, this->swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);


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

void VulkanEngine::draw_background(VkCommandBuffer cmd)
{
  ComputeEffect& effect = this->backgroundEffects[this->currentBackgroundEffect];

  // bind the gradient drawing compute pipeline
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

	// bind the descriptor set containing the draw image for the compute pipeline
	vkCmdBindDescriptorSets(
    cmd,
    VK_PIPELINE_BIND_POINT_COMPUTE,
    this->gradientPipelineLayout,
    0,
    1,
    &this->drawImageDescriptors,
    0,
    nullptr
  );

  vkCmdPushConstants(cmd, this->gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	vkCmdDispatch(cmd, std::ceil(this->drawExtent.width / 16.0), std::ceil(this->drawExtent.height / 16.0), 1);
}

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
  VkRenderingAttachmentInfo colorAttachment = vkinit::attachemnt_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = vkinit::rendering_info(this->swapchainExtent, &colorAttachment, nullptr);

	vkCmdBeginRendering(cmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
}


void VulkanEngine::run() {
  SDL_Event e;
  bool quit = false;
  bool stopRendering = false;

  Uint64 start = SDL_GetTicks();
  int frameCounter = 0;
  while (!quit) {
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT) quit = true;

      if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) quit = true;

      if(e.type == SDL_EVENT_WINDOW_MINIMIZED) {
        stopRendering = true;
      }

      if (e.type == SDL_EVENT_WINDOW_RESTORED) {
        stopRendering = false;
      }

      ImGui_ImplSDL3_ProcessEvent(&e);
    }

    if (stopRendering) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (ImGui::Begin("background")) {
      ComputeEffect& selected = this->backgroundEffects[this->currentBackgroundEffect];

      ImGui::Text("Selected effect: %s", selected.name);

      ImGui::SliderInt("Effect Index", &this->currentBackgroundEffect, 0, this->backgroundEffects.size() - 1);
      
      ImGui::InputFloat4("data1",(float*)& selected.data.data1);
			ImGui::InputFloat4("data2",(float*)& selected.data.data2);

    }

    ImGui::End();

    //ImGui::ShowDemoWindow();

    ImGui::Render();

    draw();

    ++frameCounter;
    Uint64 now = SDL_GetTicks();
    if ((now - start) >= 1000) {
      log("frame rate = ", frameCounter); 
      frameCounter = 0;
      start = now;
    }
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



