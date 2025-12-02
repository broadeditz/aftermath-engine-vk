#pragma once

#include "vulkan/vulkan_raii.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "context.hpp"

class SwapChainManager {
public:
	SwapChainManager() = default;
    ~SwapChainManager();

    void init(VulkanContext& ctx, int width, int height, VkSurfaceKHR surface);

    void createSwapChain(VulkanContext& ctx, int width, int height);
    void recreateSwapChain(VulkanContext& ctx, int width, int height);
    void cleanupSwapChain();
    void createImageViews(VulkanContext& ctx);

    const vk::raii::SwapchainKHR& getSwapChain() const;
    std::vector<vk::Image> getSwapChainImages();
    const std::vector<vk::raii::ImageView>& getSwapChainImageViews() const;
    vk::SurfaceFormatKHR getSwapChainSurfaceFormat();
    vk::Extent2D getSwapChainExtent();

private:
	vk::raii::SwapchainKHR swapChain = nullptr;
	std::vector<vk::Image> swapChainImages;
	vk::SurfaceFormatKHR swapChainSurfaceFormat;
	vk::Extent2D swapChainExtent;
	std::vector<vk::raii::ImageView> swapChainImageViews;

    VkSurfaceKHR surface = VK_NULL_HANDLE; // non-owning handle to the surface
};
