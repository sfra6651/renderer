#include <cstdlib>
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include<GLFW/glfw3.h> //pulls in vulkan because of GLFW_INCLUDE_VULKAN flag
#include <vector>

#include "lib/osScaling.h"
#include "lib/utils.h"

inline void buildVkInstance(VkInstance& instance) {
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

inline void getGpuHandle(VkDevice& vkDevice, VkQueue& queue, const VkSurfaceKHR& surface ,const VkInstance& instance) {
    VkPhysicalDevice physicalDevice {}; // PhysicalDevice is just a handle to the hardware
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

    if(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &vkDevice) != VK_SUCCESS) {
        logErr("Failed to create logical device");
        std::exit(EXIT_FAILURE);
    }

    vkGetDeviceQueue(vkDevice, graphicsFamily, 0, &queue);
    log("Logical device created");
}

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
    int windowWidth = w*osScale/2; //half size
    int windowHeight = h*osScale/2;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "My vulkan Renderer", nullptr, nullptr);
    glfwSetWindowPos(window, x, y);
    if (!window) {
        logErr("Failed to create window");
        glfwTerminate();
        return EXIT_FAILURE;
    }


    VkInstance vkInstance {};
    VkSurfaceKHR surface {};
    buildVkInstance(vkInstance);

    if (glfwCreateWindowSurface(vkInstance, window, nullptr, &surface) != VK_SUCCESS) {
        logErr("Failed to create window surface");
        return EXIT_FAILURE;
    }

    VkDevice device {};
    VkQueue queue {};
    getGpuHandle(device, queue, surface, vkInstance);



    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(vkInstance, surface, nullptr);
    vkDestroyInstance(vkInstance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
