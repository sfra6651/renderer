#include "vulkan/vulkan.hpp"
#include <array>
#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <optional>
#include <utility>
#include <vulkan/vulkan_raii.hpp>
#define GLFW_INCLUDE_VULKAN
#include<GLFW/glfw3.h> //pulls in vulkan because of GLFW_INCLUDE_VULKAN flag
#include <glm/glm.hpp>
#include <vector>

#include "lib/osScaling.h"
#include "lib/utils.h"
#include "myVulkanHelpers.h"


struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription()
    {
        return {.binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex};
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        return {{{.location = 0, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(Vertex, pos)},
                 {.location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, color)}}};
    }
};


const std::vector<Vertex> vertices = {
    {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
};


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
    vk::raii::PipelineLayout pipelineLayout = nullptr;
    vk::raii::Pipeline graphicsPipeline = nullptr;
    vk::raii::Buffer vertexBuffer = nullptr;
    vk::raii::DeviceMemory vertexBufferMemory = nullptr;
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
        createGraphicsPipeline();
        createCommandPool();
        createVertexBuffer();
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


        vk::PhysicalDeviceFeatures2 deviceFeatures2 {};
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


    void createImageViews()
    {
        assert(swapChainImageViews.empty());

        vk::ImageViewCreateInfo imageViewCreateInfo{
            .viewType = vk::ImageViewType::e2D,
            .format = swapChainSurfaceFormat.format,
            .subresourceRange = {
                vk::ImageAspectFlagBits::eColor,
                0,
                1,
                0,
                1
            }
        };

        for (auto &image : swapChainImages) {
            imageViewCreateInfo.image = image;
            swapChainImageViews.emplace_back(logicalDevice, imageViewCreateInfo);
        }
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
            .frontFace = vk::FrontFace::eClockwise,
            .depthBiasEnable = VK_FALSE,
            .lineWidth = 1.0f
        };

        vk::PipelineMultisampleStateCreateInfo multisampling {
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = VK_FALSE
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
            .setLayoutCount = 0,
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
                .pColorBlendState = &colorBlending,
                .pDynamicState = &dynamicState,
                .layout = pipelineLayout,
                .renderPass = nullptr
            },
            {
                .colorAttachmentCount = 1,
                .pColorAttachmentFormats = &swapChainSurfaceFormat.format
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


    void copyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size) 
    {
        vk::CommandBufferAllocateInfo allocInfo { 
            .commandPool = commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1 
        };
        vk::raii::CommandBuffer commandCopyBuffer = std::move(logicalDevice.allocateCommandBuffers(allocInfo).front()); 

        commandCopyBuffer.begin(vk::CommandBufferBeginInfo { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

        commandCopyBuffer.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));

        commandCopyBuffer.end();

        queue.submit(vk::SubmitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*commandCopyBuffer }, nullptr);
        queue.waitIdle();
    }


    void createVertexBuffer() 
    {
        vk::DeviceSize bufferSize = sizeof(Vertex) * vertices.size();

        vk::BufferCreateInfo stagingInfo{ .size = bufferSize, .usage = vk::BufferUsageFlagBits::eTransferSrc, .sharingMode = vk::SharingMode::eExclusive };
        vk::raii::Buffer stagingBuffer(logicalDevice, stagingInfo);
        vk::raii::DeviceMemory stagingBufferMemory = nullptr;

        createBuffer(
            bufferSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            stagingBuffer,
            stagingBufferMemory
        );

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
        uint32_t imageIndex,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout,
        vk::AccessFlags2 src_access_mask,
        vk::AccessFlags2 dst_access_mask,
        vk::PipelineStageFlags2 src_stage_mask,
        vk::PipelineStageFlags2 dst_stage_mask)
    {
        vk::ImageMemoryBarrier2 barrier = {
            .srcStageMask = src_stage_mask,
            .srcAccessMask = src_access_mask,
            .dstStageMask = dst_stage_mask,
            .dstAccessMask = dst_access_mask,
            .oldLayout = old_layout,
            .newLayout = new_layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = swapChainImages[imageIndex],
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
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
            imageIndex,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal,
            {},
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput
        );

        vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
        vk::RenderingAttachmentInfo attachmentInfo = {
            .imageView = swapChainImageViews[imageIndex],
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = clearColor
        };

        vk::RenderingInfo renderingInfo = {
            .renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachmentInfo
        };

        cmd.beginRendering(renderingInfo);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);

        cmd.bindVertexBuffers(0, *vertexBuffer, {0});

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

        cmd.draw(3, 1, 0, 0);

        cmd.endRendering();

        // After rendering, transition the swapChain image to vk::ImageLayout::ePresentSrcKHR
        transitionImageLayout(
            cmd,
            imageIndex,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            {},
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eBottomOfPipe
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
