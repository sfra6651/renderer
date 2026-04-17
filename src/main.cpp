#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_enums.hpp"
#include <array>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <limits>
#include <map>
#include <optional>
#include <utility>
#include <vulkan/vulkan_raii.hpp>
#define GLFW_INCLUDE_VULKAN
#include<GLFW/glfw3.h> //pulls in vulkan because of GLFW_INCLUDE_VULKAN flag
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

#include "lib/osScaling.h"
#include "lib/utils.h"
#include "myVulkanHelpers.h"

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
const std::string MODEL_PATH = "src/models/viking_room.obj";
const std::string TEXTURE_PATH = "src/textures/viking_room.png";

struct Vertex
{
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec2 texCoord;

  static vk::VertexInputBindingDescription getBindingDescription()
  {
    return {.binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex};
  }

   static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions() {
        return {
            vk::VertexInputAttributeDescription( 0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos) ),
            vk::VertexInputAttributeDescription( 1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color) ),
            vk::VertexInputAttributeDescription( 2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord) )
        };
    }

  bool operator==(const Vertex& other) const {
    return pos == other.pos && color == other.color && texCoord == other.texCoord;
  }
};

struct UniformBufferObject {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
};

namespace std {
  template<> struct hash<Vertex> {
    size_t operator()(Vertex const& vertex) const {
      return ((hash<glm::vec3>()(vertex.pos) ^
              (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
              (hash<glm::vec2>()(vertex.texCoord) << 1);
      }
  };
}


class App {

public:
  void run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanUp();
  }


private:

  int WINDOW_WIDTH {};
  int WINDOW_HEIGHT {};
  GLFWwindow* window {};

  vk::raii::Instance instance = nullptr;
  vk::raii::Context context {};
  vk::raii::PhysicalDevice physicalDevice = nullptr;
  vk::raii::SurfaceKHR surface = nullptr;
  vk::raii::Device logicalDevice = nullptr;

  vk::raii::Queue queue = nullptr;
  uint32_t queueFamilyIndex = UINT32_MAX;

  vk::raii::SwapchainKHR swapChain = nullptr;
  vk::Extent2D swapChainExtent {};
  vk::SurfaceFormatKHR swapChainSurfaceFormat {};
  std::vector<vk::Image> swapChainImages {};
  std::vector<vk::raii::ImageView> swapChainImageViews {};

  vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;

  vk::raii::PipelineLayout pipelineLayout = nullptr;
  vk::raii::Pipeline graphicsPipeline = nullptr;

  uint32_t mipLevels;
  vk::raii::Image textureImage = nullptr;
  vk::raii::DeviceMemory textureImageMemory = nullptr;
  vk::raii::ImageView textureImageView = nullptr;
  vk::raii::Sampler textureSampler = nullptr;

  vk::raii::Image depthImage = nullptr;
  vk::raii::DeviceMemory depthImageMemory = nullptr;
  vk::raii::ImageView depthImageView = nullptr;

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  vk::raii::Buffer vertexBuffer = nullptr;
  vk::raii::DeviceMemory vertexBufferMemory = nullptr;
  vk::raii::Buffer indexBuffer = nullptr;
  vk::raii::DeviceMemory indexBufferMemory = nullptr;
  std::vector<vk::raii::Buffer> uniformBuffers;
  std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
  std::vector<void*> uniformBuffersMapped;
  vk::raii::DescriptorPool descriptorPool = nullptr;
  std::vector<vk::raii::DescriptorSet> descriptorSets;

  vk::raii::CommandPool commandPool = nullptr;
  std::vector<vk::raii::CommandBuffer> commandBuffers {};


  //semaphores
  std::vector<vk::raii::Semaphore> presentCompleteSemaphores {};
  std::vector<vk::raii::Semaphore> renderFinishedSemaphores {};
  std::vector<vk::raii::Fence> inFlightFences {};

  uint32_t MAX_FRAMES_IN_FLIGHT = 2;
  uint32_t frameIndex = 0;
  bool frameBufferResized = false;



  void initVulkan()
  {
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createCommandPool();
    createDepthRescources();
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
    loadModel();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
  }


  void mainLoop()
  {
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
      drawFrame();
    }
    logicalDevice.waitIdle();
  }

  void updateUniformBuffer(uint32_t currentImage) 
  {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};

    ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
//    ubo.model = glm::mat4(1.0f);
    ubo.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(
      glm::radians(45.0f),
      static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height),
      0.1f, 10.0f
    );
    ubo.proj[1][1] *= -1;

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
  }

  void drawFrame()
  {
    auto fenceResult = logicalDevice.waitForFences(*inFlightFences[frameIndex], VK_TRUE, UINT64_MAX);
    if (fenceResult != vk::Result::eSuccess) {
      logErr("Failed to wait for fence!");
      std::exit(EXIT_FAILURE);
    }

    uint32_t imageIndex;
    try {
      auto [acquireResult, idx] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);
      imageIndex = idx;
    } catch (const vk::OutOfDateKHRError&) {
      recreateSwapChain();
      return;
    }
    logicalDevice.resetFences(*inFlightFences[frameIndex]);

    updateUniformBuffer(frameIndex);

    commandBuffers[frameIndex].reset();
    recordCommandBuffer(imageIndex);

    vk::PipelineStageFlags waitDestinationStageMask( vk::PipelineStageFlagBits::eColorAttachmentOutput );
    const vk::SubmitInfo   submitInfo {
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &*presentCompleteSemaphores[frameIndex],
      .pWaitDstStageMask = &waitDestinationStageMask,
      .commandBufferCount = 1,
      .pCommandBuffers = &*commandBuffers[frameIndex],
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &*renderFinishedSemaphores[imageIndex]
    };

    const vk::PresentInfoKHR presentInfoKHR {
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &*renderFinishedSemaphores[imageIndex],
      .swapchainCount = 1,
      .pSwapchains = &*swapChain,
      .pImageIndices = &imageIndex
    };

    queue.submit(submitInfo, *inFlightFences[frameIndex]);

    vk::Result presentResult = vk::Result::eSuccess;
    try {
      presentResult = queue.presentKHR(presentInfoKHR);
    } catch (const vk::OutOfDateKHRError&) {
      presentResult = vk::Result::eErrorOutOfDateKHR;
    }
    if (presentResult == vk::Result::eSuboptimalKHR || presentResult == vk::Result::eErrorOutOfDateKHR || frameBufferResized) {
      frameBufferResized = false;
      recreateSwapChain();
    }

    frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
  }

  void cleanupSwapChain()
  {
    swapChainImageViews.clear();
    swapChain = nullptr;
  }


  void recreateSwapChain()
  {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
      glfwGetFramebufferSize(window, &width, &height);
      glfwWaitEvents();
    }

    logicalDevice.waitIdle();

    cleanupSwapChain();
    createSwapChain();
    createImageViews();
    createDepthRescources();
    createRenderFinishedSemaphores();
    log("Swap chain recreated");
  }


  void cleanUp()
  {
    glfwDestroyWindow(window);
    glfwTerminate();
  }


  void initWindow()
  {
    if (!glfwInit()) {
      logErr("Failed to initialize GLFW");
      std::exit(EXIT_FAILURE);
    }

    int monitorCount {};
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    GLFWmonitor* monitor = monitorCount > 1 ? monitors[1] : monitors[0];
    int x, y, w, h;
    glfwGetMonitorWorkarea(monitor, &x, &y, &w, &h);
    float osScale = getOsScaleFactor();
    WINDOW_WIDTH = static_cast<int>(static_cast<float>(w) * osScale/2.0f);
    WINDOW_HEIGHT = static_cast<int>(static_cast<float>(h) * osScale/2.0f);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "My vulkan Renderer", nullptr, nullptr);

    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetWindowUserPointer(window, this);

    glfwSetWindowPos(window, x, y);
    if (!window) {
      logErr("Failed to create window");
      glfwTerminate();
      std::exit(EXIT_FAILURE);
    }
  }

  static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
    app->frameBufferResized = true;
  }


  void createInstance()
  {
  #ifdef DEBUG
    if (!checkValidationLayerSupport()) {
      logErr("Validation layers requested, but not available");
      std::exit(EXIT_FAILURE);
    }
  #endif
    vk::ApplicationInfo appInfo {
      .pApplicationName = "My vulkan Renderer",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = vk::ApiVersion13,
    };

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    vk::InstanceCreateInfo createInfo {
      .pApplicationInfo= &appInfo,
      .enabledExtensionCount= glfwExtensionCount,
      .ppEnabledExtensionNames= glfwExtensions,
    };

  #ifdef DEBUG
    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
  #else
    createInfo.enabledLayerCount = 0;
  #endif

    instance = vk::raii::Instance(context, createInfo);
    log("Vulkan instance created");
  }


  void createSurface()
  {
    VkSurfaceKHR _surface {};
    if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != VK_SUCCESS) {
      logErr("Failed to create window surface");
      std::exit(EXIT_FAILURE);
    }
    surface = vk::raii::SurfaceKHR(instance, _surface);
  }


  void pickPhysicalDevice()
  {
    auto physicalDevices = instance.enumeratePhysicalDevices();
    if (physicalDevices.empty()) {
      logErr("Failed to find GPUs with Vulkan support");
      std::exit(EXIT_FAILURE);
    }

    // Use an ordered map to automatically sort candidates by increasing score
    // stores a pair of score and device
    std::multimap<int, vk::raii::PhysicalDevice> candidates;

    for (const auto& device : physicalDevices) {
      uint64_t score = rateDevice(device, surface);
      candidates.insert(std::make_pair(score, device));
    }

    // chack the last(highest) score is suitable
    if (candidates.rbegin()->first > 0) {
      physicalDevice = std::move(candidates.rbegin()->second);
    } else {
      logErr("Failed to find suitable GPU");
      std::exit(EXIT_FAILURE);
    }

    if (physicalDevice == VK_NULL_HANDLE) {
      logErr("Failed to find suitable GPU");
      std::exit(EXIT_FAILURE);
    }

    log("Found physical device: ", physicalDevice.getProperties().deviceName);
  }


  void createLogicalDevice()
  {
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
    for (uint64_t i = 0; i < queueFamilyProperties.size(); i++) {
      // require the queue to support both graphics and present
      if (queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics &&
        physicalDevice.getSurfaceSupportKHR(i, surface)) {
        queueFamilyIndex = i;
      }
    }
    if (queueFamilyIndex == UINT32_MAX) {
      logErr("Failed to find a suitable queue family");
      std::exit(EXIT_FAILURE);
    }

    float queuePriority = 0.5f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo {
      .queueFamilyIndex = queueFamilyIndex,
      .queueCount = 1,
      .pQueuePriorities = &queuePriority
    };


    vk::PhysicalDeviceFeatures2 deviceFeatures2 {
      .features = {.samplerAnisotropy = true } 
    };
    vk::PhysicalDeviceVulkan11Features deviceVulkan11Features {
      .shaderDrawParameters = true,
    };
    vk::PhysicalDeviceVulkan13Features deviceVulkan13Features {
      .synchronization2 = true,
      .dynamicRendering = true,
    };
    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT deviceExtendedDynamicStateFeaturesEXT {
      .extendedDynamicState = true,
    };
    vk::StructureChain featureChain = {
      deviceFeatures2,
      deviceVulkan11Features,
      deviceVulkan13Features,
      deviceExtendedDynamicStateFeaturesEXT,
    };

    vk::DeviceCreateInfo deviceCreateInfo {
      .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &deviceQueueCreateInfo,
      .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
      .ppEnabledExtensionNames = requiredExtensions.data()
    };

    logicalDevice = vk::raii::Device(physicalDevice, deviceCreateInfo);

    queue = vk::raii::Queue(logicalDevice, queueFamilyIndex, 0);

    log("Created logical device");

  }


  void createSwapChain()
  {
    vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    swapChainExtent = chooseSwapExtent(surfaceCapabilities, window);
    uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities);

    std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(surface);
    swapChainSurfaceFormat = chooseSwapSurfaceFormat(availableFormats);

    SwapChainSupportDetails swapChainSupportDetails = querySwapChainSupport(physicalDevice, surface);

    vk::SwapchainCreateInfoKHR createInfo {
      .surface = surface,
      .minImageCount = minImageCount,
      .imageFormat = swapChainSurfaceFormat.format,
      .imageColorSpace = swapChainSurfaceFormat.colorSpace,
      .imageExtent = swapChainExtent,
      .imageArrayLayers = 1,    //always 1 unless 3D
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,// need to change this for post proccessing
      .imageSharingMode = vk::SharingMode::eExclusive,
      .preTransform = surfaceCapabilities.currentTransform,
      .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
      .presentMode = chooseSwapPresentMode(swapChainSupportDetails.availablePresentModes),
      .clipped = true
    };

    swapChain = vk::raii::SwapchainKHR(logicalDevice, createInfo);
    log("SwapChain created");

    swapChainImages = swapChain.getImages();
  }


  vk::raii::ImageView createImageView(uint32_t mipLevels, vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags) 
  {
    vk::ImageViewCreateInfo viewInfo { .image = image,
      .viewType = vk::ImageViewType::e2D,
      .format = format,
      .subresourceRange = {
        .aspectMask = aspectFlags,
        .baseMipLevel = 0,
        .levelCount = mipLevels,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
    };
    return vk::raii::ImageView( logicalDevice, viewInfo );
  }


  void createImageViews()
  {
    assert(swapChainImageViews.empty());

    for (auto &image : swapChainImages) {
      swapChainImageViews.push_back(createImageView(1, image, swapChainSurfaceFormat.format, vk::ImageAspectFlagBits::eColor));
    }
  }


  void createTextureImageView() 
  {
    textureImageView = createImageView(mipLevels, textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
  }


  void createTextureSampler() 
  {
    vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();

    vk::SamplerCreateInfo samplerInfo {
      .magFilter = vk::Filter::eLinear,
      .minFilter = vk::Filter::eLinear,
      .mipmapMode = vk::SamplerMipmapMode::eLinear,
      .addressModeU = vk::SamplerAddressMode::eRepeat,
      .addressModeV = vk::SamplerAddressMode::eRepeat,
      .addressModeW = vk::SamplerAddressMode::eRepeat,
      .mipLodBias = 0.0f,
      .anisotropyEnable = vk::True,
      .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
      .compareEnable = vk::False,
      .compareOp = vk::CompareOp::eAlways,
      .minLod = 0.0f,
      .maxLod = 0.0f,
      .borderColor = vk::BorderColor::eIntOpaqueBlack,
      .unnormalizedCoordinates = vk::False,
    };

    textureSampler = vk::raii::Sampler(logicalDevice, samplerInfo);
  }


  void createDescriptorSetLayout() 
  {
    std::array bindings = {
      vk::DescriptorSetLayoutBinding( 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr),
      vk::DescriptorSetLayoutBinding( 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
    };


    vk::DescriptorSetLayoutCreateInfo layoutInfo{
      .bindingCount = bindings.size(),
      .pBindings = bindings.data()
    };

    descriptorSetLayout = vk::raii::DescriptorSetLayout(logicalDevice, layoutInfo);



  }


  void createGraphicsPipeline()
  {
    std::vector<char> shaderCode = readFile("src/shaders/slang.spv");
    vk::raii::ShaderModule shaderModule = createShaderModule(logicalDevice, shaderCode);


    vk::PipelineShaderStageCreateInfo vertShaderStageInfo {
      .stage = vk::ShaderStageFlagBits::eVertex,
      .module = *shaderModule,
      .pName = "vertMain"
    };

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo {
      .stage = vk::ShaderStageFlagBits::eFragment,
      .module = *shaderModule,
      .pName = "fragMain"
    };
    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

    vk::PipelineDynamicStateCreateInfo dynamicState {
      .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
      .pDynamicStates = dynamicStates.data()
    };

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly {
      .topology = vk::PrimitiveTopology::eTriangleList
    };


    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo {
      .vertexBindingDescriptionCount = 1,
       .pVertexBindingDescriptions = &bindingDescription,
       .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
       .pVertexAttributeDescriptions = attributeDescriptions.data()};

    vk::PipelineViewportStateCreateInfo viewportState {
      .viewportCount = 1,
      .scissorCount  = 1,
    };

    vk::PipelineRasterizationStateCreateInfo rasterizer {
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = vk::PolygonMode::eFill,
      .cullMode = vk::CullModeFlagBits::eBack,
      .frontFace = vk::FrontFace::eCounterClockwise,
      .depthBiasEnable = VK_FALSE,
      .lineWidth = 1.0f
    };

    vk::PipelineMultisampleStateCreateInfo multisampling {
      .rasterizationSamples = vk::SampleCountFlagBits::e1,
      .sampleShadingEnable = VK_FALSE
    };

    vk::PipelineDepthStencilStateCreateInfo depthStencil{
      .depthTestEnable       = vk::True,
      .depthWriteEnable      = vk::True,
      .depthCompareOp        = vk::CompareOp::eLess,
      .depthBoundsTestEnable = vk::False,
      .stencilTestEnable     = vk::False
    };

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
      .blendEnable         = VK_TRUE,
      .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
      .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
      .colorBlendOp        = vk::BlendOp::eAdd,
      .srcAlphaBlendFactor = vk::BlendFactor::eOne,
      .dstAlphaBlendFactor = vk::BlendFactor::eZero,
      .alphaBlendOp        = vk::BlendOp::eAdd,
      .colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    };

    vk::PipelineColorBlendStateCreateInfo colorBlending {
      .logicOpEnable = VK_FALSE,
      .logicOp = vk::LogicOp::eCopy,
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment
    };

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo {
      .setLayoutCount = 1,
      .pSetLayouts = &*descriptorSetLayout,
      .pushConstantRangeCount = 0
    };

    pipelineLayout = vk::raii::PipelineLayout(logicalDevice, pipelineLayoutInfo);
    log("Created pipeline layout");

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
      {
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState  = &depthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = pipelineLayout,
        .renderPass = nullptr,
      },
      {
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapChainSurfaceFormat.format,
        .depthAttachmentFormat = findDepthFormat(physicalDevice),
      }
    };

    graphicsPipeline = vk::raii::Pipeline(logicalDevice, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
    log("Created graphics pipeline");
  }


  void createBuffer(
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties,
    vk::raii::Buffer& buffer,
    vk::raii::DeviceMemory& bufferMemory
  ) {
    vk::BufferCreateInfo bufferInfo { 
      .size = size,
      .usage = usage,
      .sharingMode = vk::SharingMode::eExclusive 
    };

    buffer = vk::raii::Buffer(logicalDevice, bufferInfo);

    vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo {
      .allocationSize = memRequirements.size,
      .memoryTypeIndex = findMemoryType(physicalDevice ,memRequirements.memoryTypeBits, properties)
    };

    bufferMemory = vk::raii::DeviceMemory(logicalDevice, allocInfo);
    buffer.bindMemory(*bufferMemory, 0);
  }


  vk::raii::CommandBuffer beginSingleTimeCommands() 
  {
    vk::CommandBufferAllocateInfo allocInfo { 
      .commandPool = commandPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1 
    };
    vk::raii::CommandBuffer commandBuffer = std::move(logicalDevice.allocateCommandBuffers(allocInfo).front());
  
    vk::CommandBufferBeginInfo beginInfo { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
    commandBuffer.begin(beginInfo);
  
    return commandBuffer;
  }


  void endSingleTimeCommands(vk::raii::CommandBuffer& commandBuffer) 
  {
    commandBuffer.end();
  
    vk::SubmitInfo submitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*commandBuffer };
    queue.submit(submitInfo, nullptr);
    queue.waitIdle();
  }


  void copyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size) 
  {
    vk::raii::CommandBuffer commandCopyBuffer = beginSingleTimeCommands();
    commandCopyBuffer.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));
    endSingleTimeCommands(commandCopyBuffer);
  }


  void copyBufferToImage(const vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height) 
  {
    vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();

    vk::BufferImageCopy region{ .bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0,
    .imageSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 }, .imageOffset = {0, 0, 0}, .imageExtent = {width, height, 1} };

    commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, {region});
    // Submit the buffer copy to the graphics queue
    endSingleTimeCommands(commandBuffer);
  }


  void createImage(
    uint32_t width,
    uint32_t height,
    uint32_t mipLevels,
    vk::Format format,
    vk::ImageTiling tiling,
    vk::ImageUsageFlags usage,
    vk::MemoryPropertyFlags properties,
    vk::raii::Image& image,
    vk::raii::DeviceMemory& imageMemory
  ) {
    vk::ImageCreateInfo imageInfo { 
      .imageType = vk::ImageType::e2D,
      .format = format,
      .extent = {width, height, 1},
      .mipLevels = mipLevels,
      .arrayLayers = 1,
      .samples = vk::SampleCountFlagBits::e1,
      .tiling = tiling,
      .usage = usage,
      .sharingMode = vk::SharingMode::eExclusive 
    };

    image = vk::raii::Image(logicalDevice, imageInfo);

    vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo{ .allocationSize = memRequirements.size,
                                        .memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties) };
    imageMemory = vk::raii::DeviceMemory(logicalDevice, allocInfo);
    image.bindMemory(imageMemory, 0);
  }


  void createDepthRescources() 
  {
    vk::Format depthFormat = findDepthFormat(physicalDevice);
    createImage(
      swapChainExtent.width,
      swapChainExtent.height,
      1,
      depthFormat,
      vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eDepthStencilAttachment,
      vk::MemoryPropertyFlagBits::eDeviceLocal,
      depthImage,
      depthImageMemory
    );
    depthImageView = createImageView(1, depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
  }


  void createTextureImage() 
  {
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str() , &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    vk::DeviceSize imageSize = texWidth * texHeight * 4;

    mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

    if (!pixels) {
      logErr("Failed to load image: ");
      log("CWD: ",std::filesystem::current_path());
    }

    vk::raii::Buffer stagingBuffer({});
    vk::raii::DeviceMemory stagingBufferMemory({});

    createBuffer(
      imageSize,
      vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      stagingBuffer,
      stagingBufferMemory
    );

    void* data = stagingBufferMemory.mapMemory(0, imageSize);
    memcpy(data, pixels, imageSize);
    stagingBufferMemory.unmapMemory();

    stbi_image_free(pixels);

    createImage(
      texWidth,
      texHeight,
      mipLevels,
      vk::Format::eR8G8B8A8Srgb,
      vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
      vk::MemoryPropertyFlagBits::eDeviceLocal,
      textureImage,
      textureImageMemory
    );

    {
      auto cmd = beginSingleTimeCommands();
      transitionImageLayout(
        cmd,
        textureImage,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal,
        {}, vk::AccessFlagBits2::eTransferWrite,
        vk::PipelineStageFlagBits2::eTopOfPipe,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::ImageAspectFlagBits::eColor,
        mipLevels
      );
      endSingleTimeCommands(cmd);
    }

    copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

    {
      auto cmd = beginSingleTimeCommands();
      transitionImageLayout(
        cmd,
        textureImage,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::AccessFlagBits2::eTransferWrite,
        vk::AccessFlagBits2::eShaderRead,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::PipelineStageFlagBits2::eFragmentShader,
        vk::ImageAspectFlagBits::eColor,
        mipLevels
      );
      endSingleTimeCommands(cmd);
    }
  }


  void loadModel() 
  {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str())) {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for (const auto& shape : shapes) {
      for (const auto& index : shape.mesh.indices) {
        Vertex vertex{};

        vertex.pos = {
          attrib.vertices[3 * index.vertex_index + 0],
          attrib.vertices[3 * index.vertex_index + 1],
          attrib.vertices[3 * index.vertex_index + 2]
        };

        vertex.texCoord = {
          attrib.texcoords[2 * index.texcoord_index + 0],
          1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
        };

        vertex.color = {1.0f, 1.0f, 1.0f};

        vertices.push_back(vertex);

        if (uniqueVertices.count(vertex) == 0) {
          uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
          vertices.push_back(vertex);
        }
    
        indices.push_back(uniqueVertices[vertex]);
      }
    }
  }

  void createVertexBuffer() 
  {
    vk::DeviceSize bufferSize = sizeof(Vertex) * vertices.size();

    vk::BufferCreateInfo stagingInfo { 
      .size = bufferSize,
      .usage = vk::BufferUsageFlagBits::eTransferSrc,
      .sharingMode = vk::SharingMode::eExclusive 
    };
    vk::raii::Buffer stagingBuffer(logicalDevice, stagingInfo);
    vk::raii::DeviceMemory stagingBufferMemory = nullptr;

    createBuffer(
      bufferSize,
      vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      stagingBuffer,
      stagingBufferMemory
    );

    //can just directly copy the data from the cpu into the staging buffer because its host visible - get handle with mapMemory
    void* dataStaging = stagingBufferMemory.mapMemory(0, stagingInfo.size);
    memcpy(dataStaging, vertices.data(), stagingInfo.size);
    stagingBufferMemory.unmapMemory();

    createBuffer(
      bufferSize,
      vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eDeviceLocal,
      vertexBuffer,
      vertexBufferMemory
    );

    copyBuffer(stagingBuffer, vertexBuffer, stagingInfo.size);
  }


  void createIndexBuffer() {
    vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    vk::raii::Buffer stagingBuffer({});
    vk::raii::DeviceMemory stagingBufferMemory({});
    createBuffer(
      bufferSize,
      vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      stagingBuffer,
      stagingBufferMemory
    );

    void* data = stagingBufferMemory.mapMemory(0, bufferSize);
    memcpy(data, indices.data(), (size_t) bufferSize);
    stagingBufferMemory.unmapMemory();

    createBuffer(
      bufferSize,
      vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
      vk::MemoryPropertyFlagBits::eDeviceLocal,
      indexBuffer,
      indexBufferMemory
    );

    copyBuffer(stagingBuffer, indexBuffer, bufferSize);
  }


  void createUniformBuffers() 
  {
    uniformBuffers.clear();
    uniformBuffersMemory.clear();
    uniformBuffersMapped.clear();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
      vk::raii::Buffer buffer({});
      vk::raii::DeviceMemory bufferMem({});
      createBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eUniformBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        buffer,
        bufferMem
      );
      uniformBuffers.emplace_back(std::move(buffer));
      uniformBuffersMemory.emplace_back(std::move(bufferMem));
      uniformBuffersMapped.emplace_back( uniformBuffersMemory[i].mapMemory(0, bufferSize));
    }
  }


  void createDescriptorPool()
  {

    std::array poolSize {
      vk::DescriptorPoolSize( vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT),
      vk::DescriptorPoolSize(  vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT)
    };

    vk::DescriptorPoolCreateInfo poolInfo { 
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = MAX_FRAMES_IN_FLIGHT,
      .poolSizeCount = poolSize.size(),
      .pPoolSizes = poolSize.data(),
    };

    descriptorPool = vk::raii::DescriptorPool(logicalDevice, poolInfo);

  }


  void createDescriptorSets() 
  {
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);

    vk::DescriptorSetAllocateInfo allocInfo { 
      .descriptorPool = descriptorPool,
      .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
      .pSetLayouts = layouts.data() 
    };

    descriptorSets.clear();

    descriptorSets = logicalDevice.allocateDescriptorSets(allocInfo);


    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vk::DescriptorBufferInfo bufferInfo { 
        .buffer = uniformBuffers[i],
        .offset = 0,
        .range = sizeof(UniformBufferObject) 
      };

      vk::DescriptorImageInfo imageInfo { 
        .sampler = textureSampler,
        .imageView = textureImageView,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
      };

      std::array descriptorWrites {
        vk::WriteDescriptorSet { 
          .dstSet = descriptorSets[i],
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .pBufferInfo = &bufferInfo 
        },
        vk::WriteDescriptorSet { 
          .dstSet = descriptorSets[i],
          .dstBinding = 1,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eCombinedImageSampler,
          .pImageInfo = &imageInfo 
        }
      };

      logicalDevice.updateDescriptorSets(descriptorWrites, {});
    }
  }


  void createCommandPool()
  {
    vk::CommandPoolCreateInfo poolInfo {
      .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queueFamilyIndex
    };

    commandPool = vk::raii::CommandPool(logicalDevice, poolInfo);
    log("Created command pool");
  }


  void createCommandBuffers()
  {
    vk::CommandBufferAllocateInfo allocInfo {
      .commandPool = commandPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
    };

    commandBuffers = vk::raii::CommandBuffers(logicalDevice, allocInfo);
    log("Created command buffers");
  }


  void transitionImageLayout(
    vk::raii::CommandBuffer& cmdBuffer,
    vk::Image image,
    vk::ImageLayout old_layout,
    vk::ImageLayout new_layout,
    vk::AccessFlags2 src_access_mask,
    vk::AccessFlags2 dst_access_mask,
    vk::PipelineStageFlags2 src_stage_mask,
    vk::PipelineStageFlags2 dst_stage_mask,
    vk::ImageAspectFlags    image_aspect_flags,
    uint32_t mipLevels
  ) {
    vk::ImageMemoryBarrier2 barrier = {
      .srcStageMask = src_stage_mask,
      .srcAccessMask = src_access_mask,
      .dstStageMask = dst_stage_mask,
      .dstAccessMask = dst_access_mask,
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = {
        .aspectMask = image_aspect_flags,
        .baseMipLevel = 0,
        .levelCount = mipLevels,
        .baseArrayLayer = 0,
        .layerCount = 1
      }
    };

    vk::DependencyInfo dependencyInfo = {
      .dependencyFlags         = {},
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers    = &barrier
    };

    cmdBuffer.pipelineBarrier2(dependencyInfo);
  }


  void recordCommandBuffer(uint32_t imageIndex)
  {
    auto& cmd = commandBuffers[frameIndex];

    cmd.begin({});

    // Before starting rendering, transition the swapchain image to vk::ImageLayout::eColorAttachmentOptimal
    transitionImageLayout(
      cmd,
      swapChainImages[imageIndex],
      vk::ImageLayout::eUndefined,
      vk::ImageLayout::eColorAttachmentOptimal,
      {},
      vk::AccessFlagBits2::eColorAttachmentWrite,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::ImageAspectFlagBits::eColor,
      1
    );

    // Transition depth image to eDepthAttachmentOptimal before rendering
    transitionImageLayout(
      cmd,
      depthImage,
      vk::ImageLayout::eUndefined,
      vk::ImageLayout::eDepthAttachmentOptimal,
      {},
      vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
      vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
      vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
      vk::ImageAspectFlagBits::eDepth,
      1
    );

    vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    vk::RenderingAttachmentInfo attachmentInfo = {
      .imageView = swapChainImageViews[imageIndex],
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = clearColor,
    };

    vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);
    vk::RenderingAttachmentInfo depthAttachmentInfo = {
      .imageView   = depthImageView,
      .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
      .loadOp      = vk::AttachmentLoadOp::eClear,
      .storeOp     = vk::AttachmentStoreOp::eDontCare,
      .clearValue  = clearDepth
    };

    vk::RenderingInfo renderingInfo = {
      .renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &attachmentInfo,
      .pDepthAttachment = &depthAttachmentInfo,
    };

    cmd.beginRendering(renderingInfo);

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);

    cmd.bindVertexBuffers(0, *vertexBuffer, {0});
    cmd.bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint32);

    cmd.setViewport(
      0,
      vk::Viewport(
        0.0f,
        0.0f,
        static_cast<float>(swapChainExtent.width),
        static_cast<float>(swapChainExtent.height),
        0.0f,
        1.0f
      )
    );
    cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

    commandBuffers[frameIndex].bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics,
      pipelineLayout,
      0,
      *descriptorSets[frameIndex],
      nullptr
    );
    commandBuffers[frameIndex].drawIndexed(indices.size(), 1, 0, 0, 0);

    cmd.drawIndexed(indices.size(), 1, 0, 0, 0);

    cmd.endRendering();

    // After rendering, transition the swapChain image to vk::ImageLayout::ePresentSrcKHR
    transitionImageLayout(
      cmd,
      swapChainImages[imageIndex],
      vk::ImageLayout::eColorAttachmentOptimal,
      vk::ImageLayout::ePresentSrcKHR,
      vk::AccessFlagBits2::eColorAttachmentWrite,
      {},
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::PipelineStageFlagBits2::eBottomOfPipe,
      vk::ImageAspectFlagBits::eColor,
      1
    );

    cmd.end();
  }


  void createRenderFinishedSemaphores()
  {
    renderFinishedSemaphores.clear();
    for (uint32_t i = 0; i < swapChainImages.size(); i++) {
      renderFinishedSemaphores.emplace_back(logicalDevice, vk::SemaphoreCreateInfo{});
    }
  }

  void createSyncObjects()
  {
    createRenderFinishedSemaphores();
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      presentCompleteSemaphores.emplace_back(logicalDevice, vk::SemaphoreCreateInfo{});
      inFlightFences.emplace_back(logicalDevice, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
    }
  }
};


int main ()
{
  App app {};
  app.run();
  return EXIT_SUCCESS;
}
