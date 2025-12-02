#include "swapchain.hpp"
#include <iostream>

SwapChainManager::~SwapChainManager() {
    cleanupSwapChain();
}

void SwapChainManager::init(VulkanContext& ctx, int width, int height, VkSurfaceKHR surface) {
    this->surface = surface;

    std::cout << "creating swapchain" << std::endl;
    createSwapChain(ctx, width, height);

    std::cout << "creating image views" << std::endl;
    createImageViews(ctx);
}

void SwapChainManager::createSwapChain(VulkanContext& ctx, int width, int height) {
    auto capabilities = ctx.getPhysicalDevice().getSurfaceCapabilitiesKHR(surface);
    auto formats = ctx.getPhysicalDevice().getSurfaceFormatsKHR(surface);
    auto presentModes = ctx.getPhysicalDevice().getSurfacePresentModesKHR(surface);

    swapChainSurfaceFormat = formats[0];
    for (const auto& format : formats) {
        if (format.format == vk::Format::eB8G8R8A8Srgb &&
            format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            swapChainSurfaceFormat = format;
            break;
        }
    }

    swapChainExtent = {
        std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
    };

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0) imageCount = std::min(imageCount, capabilities.maxImageCount);

    vk::SwapchainCreateInfoKHR createInfo{
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = swapChainSurfaceFormat.format,
        .imageColorSpace = swapChainSurfaceFormat.colorSpace,
        .imageExtent = swapChainExtent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = vk::PresentModeKHR::eFifo,
        .clipped = vk::True
    };

    swapChain = ctx.getDevice().createSwapchainKHR(createInfo);
    swapChainImages = swapChain.getImages();
}

void SwapChainManager::recreateSwapChain(VulkanContext& ctx, int width, int height) {
    ctx.getDevice().waitIdle();
    cleanupSwapChain();
    createSwapChain(ctx, width, height);
    createImageViews(ctx);
}

void SwapChainManager::cleanupSwapChain() {
    swapChainImageViews.clear();
    swapChain = nullptr;
}

void SwapChainManager::createImageViews(VulkanContext& ctx) {
    swapChainImageViews.clear();
    for (auto image : swapChainImages) {
        vk::ImageViewCreateInfo createInfo{
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = swapChainSurfaceFormat.format,
            .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
        };
        swapChainImageViews.emplace_back(ctx.getDevice(), createInfo);
    }
}

vk::Extent2D SwapChainManager::getSwapChainExtent() {
	return swapChainExtent;
}

vk::SurfaceFormatKHR SwapChainManager::getSwapChainSurfaceFormat() {
	return swapChainSurfaceFormat;
}

const vk::raii::SwapchainKHR& SwapChainManager::getSwapChain() const {
	return swapChain;
}

const std::vector<vk::raii::ImageView>& SwapChainManager::getSwapChainImageViews() const {
	return swapChainImageViews;
}

std::vector<vk::Image> SwapChainManager::getSwapChainImages() {
	return swapChainImages;
}
