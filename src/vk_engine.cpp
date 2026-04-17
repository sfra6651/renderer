#include "vk_engine.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>

#include "lib/utils.h"

namespace {

constexpr bool kUseValidationLayers =
#ifdef DEBUG
    true;
#else
    false;
#endif

}  // namespace

void VulkanEngine::init() {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    logErr("SDL_Init failed:", SDL_GetError());
    std::abort();
  }

  window_ = SDL_CreateWindow("renderer",
                             static_cast<int>(window_extent_.width),
                             static_cast<int>(window_extent_.height),
                             SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  if (!window_) {
    logErr("SDL_CreateWindow failed:", SDL_GetError());
    std::abort();
  }

  init_vulkan();
  is_initialized_ = true;
}

void VulkanEngine::init_vulkan() {
  vkb::InstanceBuilder builder;
  auto inst_ret = builder.set_app_name("renderer")
                      .request_validation_layers(kUseValidationLayers)
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
  instance_ = vkb_inst.instance;
  debug_messenger_ = vkb_inst.debug_messenger;

  if (!SDL_Vulkan_CreateSurface(window_, instance_, nullptr, &surface_)) {
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
  auto phys_ret = selector.set_minimum_version(1, 3)
                      .set_required_features_13(features13)
                      .set_required_features_12(features12)
                      .set_surface(surface_)
                      .select();
  if (!phys_ret) {
    logErr("vk-bootstrap GPU selection failed:", phys_ret.error().message());
    logErr("If on macOS, verify MoltenVK supports Vulkan 1.3 with dynamic rendering + sync2.");
    std::abort();
  }
  vkb::PhysicalDevice phys = phys_ret.value();

  vkb::DeviceBuilder device_builder{phys};
  auto dev_ret = device_builder.build();
  if (!dev_ret) {
    logErr("vk-bootstrap device build failed:", dev_ret.error().message());
    std::abort();
  }
  vkb::Device vkb_device = dev_ret.value();
  device_ = vkb_device.device;
  chosen_gpu_ = phys.physical_device;

  log("Vulkan initialized:", phys.name);
}

void VulkanEngine::run() {
  SDL_Event e;
  bool quit = false;
  while (!quit) {
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT) quit = true;
      if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) quit = true;
    }
    ++frame_number_;
  }
}

void VulkanEngine::cleanup() {
  if (!is_initialized_) return;

  if (device_) vkDestroyDevice(device_, nullptr);
  if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
  if (debug_messenger_) vkb::destroy_debug_utils_messenger(instance_, debug_messenger_);
  if (instance_) vkDestroyInstance(instance_, nullptr);

  if (window_) SDL_DestroyWindow(window_);
  SDL_Quit();
}
