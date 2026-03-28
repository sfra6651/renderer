#include <cstdlib>
#include <utility>
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include<GLFW/glfw3.h> //pulls in vulkan because of GLFW_INCLUDE_VULKAN flag
#include <vector>

#include "lib/osScaling.h"
#include "lib/utils.h"


struct Vulkan {
    //no items we dont need to manually clean up
    VkQueue queue {};
    VkFormat swapChainFormat {};    // VkFromat is a member of VKSurfaceFormatKHR that we use in buildSwapChain. 
                                    //we only need .colorSpace from VkSurfaceFormat when building the swapchain
    VkExtent2D extent {}; 

    //members that need cleaned up
    VkInstance instance {};
    VkSurfaceKHR surface {};
    VkPhysicalDevice physicalDevice {}; // for quering info about the hardware
    VkDevice logicalDevice {};          // for the working connection to the GPU
    VkSwapchainKHR swapchain {};
    std::vector<VkImageView> swapchainImageViews;
    VkRenderPass renderPass {};
    std::vector<VkFramebuffer> frameBuffers {};



    void cleanUp() {
        for (auto fb : frameBuffers) {
            vkDestroyFramebuffer(logicalDevice, fb, nullptr);
        }
        vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
        for (auto view : swapchainImageViews) {
            vkDestroyImageView(logicalDevice, view, nullptr);
        }
        vkDestroySwapchainKHR(logicalDevice, swapchain, nullptr);
        vkDestroyDevice(logicalDevice, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
    }

    void buildInstance() {
        VkApplicationInfo appInfo {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "My vulkan Renderer";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.apiVersion = VK_API_VERSION_1_0;
    
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    
        // Validation layer - catches API misuse in debug builds
        const char* validationLayer = "VK_LAYER_KHRONOS_validation";
    
        VkInstanceCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;
    #ifdef DEBUG
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = &validationLayer;
    #else
        createInfo.enabledLayerCount = 0;
    #endif
    
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            logErr("Failed to create vulkan instance");
            std::exit(EXIT_FAILURE);
        }
        log("Vulkan instance created");
    }

    void buildGpuHandle() {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance, &count, nullptr);
        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance, &count, devices.data());

        // Find device that has a queue family supporting both graphic and present
        uint32_t graphicsFamily = 0;
        //TODO: currenlty we just pick the first suitable one then move on. for my setup this gets my 5090 but will need to fix this at some point
        // to hanle picking the best one via vkGetPhysicalDeviceProperties(device, &props)
        for (const auto& device : devices) {
            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

            for(uint32_t i = 0; i < queueFamilyCount; i++) {
                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

                if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
                    physicalDevice = device;
                    graphicsFamily = i;
                    break;
                }
            }
            if (physicalDevice != VK_NULL_HANDLE) { break; }
        }
        if(physicalDevice == VK_NULL_HANDLE) {
            logErr("No suitable GPU found");
            std::exit(EXIT_FAILURE);
        }
        VkPhysicalDeviceProperties deviceProperties {};
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
        log("GPU: ", deviceProperties.deviceName);

        //now build device from the physicalDevice
        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = graphicsFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        // Swapchain extension
        const char* extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        
        VkDeviceCreateInfo deviceCreateInfo {};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.enabledExtensionCount = 1;
        deviceCreateInfo.ppEnabledExtensionNames = extensions;

        if(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice) != VK_SUCCESS) {
            logErr("Failed to create logical device");
            std::exit(EXIT_FAILURE);
        }

        vkGetDeviceQueue(logicalDevice, graphicsFamily, 0, &queue);
        log("Logical device created");
        } 
   
    void buildSwapChain() {
        VkSurfaceCapabilitiesKHR capabilities {};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

        // surface format - BGRA* sRGB is standard
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

        VkSurfaceFormatKHR surfaceFormat = formats[0]; // default fallback if loops below fails to find preference
        for (const auto& f : formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                swapChainFormat = f.format;
                break;
            }
        }
        swapChainFormat = surfaceFormat.format;

        // pick present mode = MAILBOX (triple buffer) else FIFO (vsync)
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());

        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // default FIFO as always available
        for (const auto& mode : presentModes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                presentMode = mode;
                break;
            }
        }

        // resolution of swapchain images
        extent = capabilities.currentExtent;

        // one more than min for tripple buffering
        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
            imageCount = capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR swapchainInfo {};
        swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainInfo.surface = surface;
        swapchainInfo.minImageCount = imageCount;
        swapchainInfo.imageFormat = surfaceFormat.format;
        swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
        swapchainInfo.imageExtent = extent;
        swapchainInfo.imageArrayLayers = 1;                                 // always 1 unless VR
        swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;     // reder these as a frambuffer color output, so the final image, no post proccessing 
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;         // one queue family owns the images, no concurrency with graphics and present queues
        swapchainInfo.preTransform = capabilities.currentTransform;         // no rotation
        swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;   // no transparency on window
        swapchainInfo.presentMode = presentMode;
        swapchainInfo.clipped = VK_TRUE;                                    // dont render pixels hidden by other windows
        swapchainInfo.oldSwapchain = VK_NULL_HANDLE;                        // not replacing old swapchain. resising the widows requires recreating he swapcahin
        
        if (vkCreateSwapchainKHR(logicalDevice, &swapchainInfo, nullptr, &swapchain)) {
            logErr("Failed to create swapchain");
            std::exit(EXIT_FAILURE);
        }

        // get handles to swapchain images created by vulkan
        vkGetSwapchainImagesKHR(logicalDevice, swapchain, &imageCount, nullptr);
        std::vector<VkImage> swapchainImages(imageCount);
        vkGetSwapchainImagesKHR(logicalDevice, swapchain, &imageCount, swapchainImages.data());

        log("Swap chain created: ", extent.width, "x", extent.height, " with" ,imageCount, " images");

        // -----Image Views------
        swapchainImageViews.resize(imageCount);

        for (uint32_t i = 0; i < swapchainImageViews.size(); i++) {
            VkImageViewCreateInfo viewInfo {};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = swapchainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = surfaceFormat.format;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;  // don't remap any channels
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;    // start at mip 0
            viewInfo.subresourceRange.levelCount = 1;      // one mip level
            viewInfo.subresourceRange.baseArrayLayer = 0;  // start at layer 0
            viewInfo.subresourceRange.layerCount = 1;      // one layer
            
            if(vkCreateImageView(logicalDevice, &viewInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
                logErr("Failed to create image view");
                std::exit(EXIT_FAILURE);
            }
        }
        log("Image views created: ", imageCount);

    }

    // the render pass defines what slots expext what images and what properties those images have
    void buildRenderPass() {
        // descripe the swapchain image
        // an attachment is a slot for an image, its confusing
        VkAttachmentDescription colorAttachment {}; 
        colorAttachment.format = swapChainFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;    // no multisampling -> antialiasing 
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;   // clear to a color at start
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;     //keep the result so we can present it
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;    // no stencil buffer
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  //dont care what was in the image before
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  //transition to presentable when done

        VkAttachmentReference colorRef {};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // best layout for writing color

        VkSubpassDescription subpass {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;

        // Synchronization: wait for the swap cahin to finish reading the image before writing to it
        VkSubpassDependency dependency {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;    // "befor render pass"
        dependency.dstSubpass = 0;                      //our subpass
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            logErr("Failed to create render pass");
            std::exit(EXIT_FAILURE);
        }
        log("Render pass created");
    }

    void buildFrameBuffers() {
        frameBuffers.resize(swapchainImageViews.size());
        for(uint32_t i = 0; i < swapchainImageViews.size(); i++) {
            VkFramebufferCreateInfo frameBufferInfo {};
            frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            frameBufferInfo.renderPass = renderPass;
            frameBufferInfo.attachmentCount = 1;
            frameBufferInfo.pAttachments = &swapchainImageViews[i];
            frameBufferInfo.width = extent.width;
            frameBufferInfo.height = extent.height;
            frameBufferInfo.layers = 1;

            if (vkCreateFramebuffer(logicalDevice, &frameBufferInfo, nullptr, &frameBuffers[i]) != VK_SUCCESS) {
                logErr("Failed to create framebuffer");
                std::exit(EXIT_FAILURE);
            }
        }
        log("Framebuffers created: ", frameBuffers.size());
    }

};


int main () {
    if (!glfwInit()) {
        logErr("Failed to initialize GLFW");
        return EXIT_FAILURE;
    }

    int monitorCount {};
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    GLFWmonitor* monitor = monitorCount > 1 ? monitors[1] : monitors[0];
    int x, y, w, h; 
    glfwGetMonitorWorkarea(monitor, &x, &y, &w, &h);
    float osScale = getOsScaleFactor();
    int windowWidth = w*osScale/2;
    int windowHeight = h*osScale/2;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "My vulkan Renderer", nullptr, nullptr);
    glfwSetWindowPos(window, x, y);
    if (!window) {
        logErr("Failed to create window");
        glfwTerminate();
        return EXIT_FAILURE;
    }


    Vulkan vk {};
    vk.buildInstance();

    if (glfwCreateWindowSurface(vk.instance, window, nullptr, &vk.surface) != VK_SUCCESS) {
        logErr("Failed to create window surface");
        return EXIT_FAILURE;
    }
    vk.buildGpuHandle();
    vk.buildSwapChain();
    vk.buildRenderPass();
    vk.buildFrameBuffers();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    vk.cleanUp();
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
