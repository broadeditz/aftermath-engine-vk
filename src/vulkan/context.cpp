#include "context.hpp"

#include <iostream>
#include <GLFW/glfw3.h>

VulkanContext::VulkanContext() {
}

VulkanContext::~VulkanContext() {
    cleanup();
}

uint32_t VulkanContext::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const {
    vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}

void VulkanContext::init(VkSurfaceKHR surface) {
    surfaceHandle = surface;

    std::cout << "Picking physical device" << std::endl;
    pickPhysicalDevice();

    std::cout << "Creating logical device" << std::endl;
    createLogicalDevice();

    std::cout << "Creating VMA allocator" << std::endl;
    createAllocator();
}


void VulkanContext::cleanup() {
    if (allocator) {
        vmaDestroyAllocator(allocator);
        allocator = nullptr;
    }
}

void VulkanContext::createInstance(bool enableValidation) {
    VULKAN_HPP_DEFAULT_DISPATCHER.init(context.getDispatcher()->vkGetInstanceProcAddr);

    vk::ApplicationInfo appInfo{
        .pApplicationName = "Eldritch Aftermath",
        .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
        .pEngineName = "Aftermath Engine",
        .engineVersion = VK_MAKE_VERSION(0, 0, 1),
        .apiVersion = vk::ApiVersion14
    };

    std::vector<const char*> requiredLayers;
    if (enableValidation) {
        requiredLayers = validationLayers;
    }

    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    vk::InstanceCreateInfo createInfo{
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
        .ppEnabledLayerNames = requiredLayers.data(),
        .enabledExtensionCount = glfwExtensionCount,
        .ppEnabledExtensionNames = glfwExtensions,
    };

    instance = vk::raii::Instance(context, createInfo);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
}

void VulkanContext::pickPhysicalDevice() {
    auto devices = instance.enumeratePhysicalDevices();
    if (devices.empty()) {
        throw std::runtime_error("No Vulkan GPUs found!");
    }

    physicalDevice = std::move(devices[0]);
    std::cout << "Using: " << physicalDevice.getProperties().deviceName << std::endl;
}

void VulkanContext::createLogicalDevice() {
    auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

    // Find graphics queue family
    graphicsIndex = 0;
    for (size_t i = 0; i < queueFamilyProperties.size(); i++) {
        if (queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            graphicsIndex = i;
            break;
        }
    }

    presentIndex = graphicsIndex;

    vk::PhysicalDeviceVulkan12Features vulkan12Features{
        .shaderInt8 = vk::True,
        .uniformAndStorageBuffer8BitAccess = vk::True,
        .storageBuffer8BitAccess = vk::True
    };

    vk::PhysicalDeviceVulkan13Features vulkan13Features{
        .pNext = &vulkan12Features,
        .synchronization2 = vk::True,
        .dynamicRendering = vk::True
    };

    float queuePriority = 1.0f;
    vk::DeviceQueueCreateInfo queueCreateInfo{
        .queueFamilyIndex = graphicsIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    };

    vk::DeviceCreateInfo deviceCreateInfo{
        .pNext = &vulkan13Features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data()
    };

    device = vk::raii::Device(physicalDevice, deviceCreateInfo);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

    // Get queue handles
    graphicsQueue = vk::raii::Queue(device, graphicsIndex, 0);
    presentQueue = graphicsQueue;  // Same queue in this case
}

void VulkanContext::createAllocator() {
    VmaAllocatorCreateInfo allocatorInfo{
        .physicalDevice = *physicalDevice,
        .device = *device,
        .instance = *instance,
        .vulkanApiVersion = VK_API_VERSION_1_4
    };

    VkResult result = vmaCreateAllocator(&allocatorInfo, &allocator);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create VMA allocator");
    }
}
