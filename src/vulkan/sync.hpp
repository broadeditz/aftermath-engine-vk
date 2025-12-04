#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <vector>

#include "context.hpp"


class SyncObjects {
public:
	SyncObjects() = default;
    ~SyncObjects();

    SyncObjects(const SyncObjects&) = delete;
    SyncObjects& operator=(const SyncObjects&) = delete;

    void create(VulkanContext& context, uint32_t swapchainImageCount, uint32_t framesInFlight);
    void cleanup();

    void nextFrame() {
        currentFrame = (currentFrame + 1) % maxFramesInFlight;
    }
    void nextSemaphore() {
        semaphoreIndex = (semaphoreIndex + 1) % semaphoreCount;
    }

    uint32_t getCurrentFrame() const {
        return currentFrame;
    }
    uint32_t getCurrentSemaphoreIndex() const {
        return semaphoreIndex;
    }

    const vk::raii::Fence& getCurrentFence() const {
        return inFlightFences[currentFrame];
    }
    const vk::raii::Semaphore& getCurrentPresentSemaphore() const {
        return presentCompleteSemaphores[semaphoreIndex];
    }
    const vk::raii::Semaphore& getCurrentRenderSemaphore() const {
        return renderFinishedSemaphores[semaphoreIndex];
    }

private:
	std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
    std::vector<vk::raii::Fence> inFlightFences;

    uint32_t currentFrame = 0;
    uint32_t semaphoreIndex = 0;
    uint32_t maxFramesInFlight = 0;
    uint32_t semaphoreCount = 0;
};
