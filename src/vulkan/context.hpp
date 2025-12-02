#pragma once

#include <vulkan/vulkan_raii.hpp>
#include "vma/vk_mem_alloc.h"

class VulkanContext {
public:
	VulkanContext();
	~VulkanContext();

	// Delete copy/move to prevent accidents with RAII objects
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&) = delete;
    VulkanContext& operator=(VulkanContext&&) = delete;

	void init(VkSurfaceKHR surface);
	void createInstance(bool enableValidation = true);
	void cleanup();

	// Getters for core objects
    const vk::raii::Instance& getInstance() const { return instance; }
    const vk::raii::PhysicalDevice& getPhysicalDevice() const { return physicalDevice; }
    const vk::raii::Device& getDevice() const { return device; }
    VmaAllocator getAllocator() const { return allocator; }

    const vk::raii::Queue& getGraphicsQueue() const { return graphicsQueue; }
    const vk::raii::Queue& getPresentQueue() const { return presentQueue; }

    uint32_t getGraphicsQueueIndex() const { return graphicsIndex; }
    uint32_t getPresentQueueIndex() const { return presentIndex; }


    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;

private:
	vk::raii::Context context;
	vk::raii::Instance instance = nullptr;
    vk::raii::PhysicalDevice physicalDevice = nullptr;
    vk::raii::Device device = nullptr;
    VmaAllocator allocator = nullptr;

    vk::raii::Queue graphicsQueue = nullptr;
    vk::raii::Queue presentQueue = nullptr;
    uint32_t graphicsIndex = 0;
    uint32_t presentIndex = 0;

    VkSurfaceKHR surfaceHandle = VK_NULL_HANDLE; // non-owning handle to the surface, for queue selection

    const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
    };

    void pickPhysicalDevice();
    void createLogicalDevice();
    void createAllocator();
};
