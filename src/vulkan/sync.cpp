#include "sync.hpp"

#include <iostream>

SyncObjects::~SyncObjects() {
	cleanup();
}

void SyncObjects::create(VulkanContext& context, uint32_t swapchainImageCount, uint32_t framesInFlight) {
	maxFramesInFlight = framesInFlight;
	semaphoreCount = swapchainImageCount;

	std::cout << "Creating sync objects" << std::endl;

	for (uint32_t i = 0; i < semaphoreCount; i++) {
		presentCompleteSemaphores.emplace_back(context.getDevice(), vk::SemaphoreCreateInfo{});
        renderFinishedSemaphores.emplace_back(context.getDevice(), vk::SemaphoreCreateInfo{});
    }

	for (uint32_t i = 0; i < maxFramesInFlight; i++) {
		inFlightFences.emplace_back(
			context.getDevice(),
            vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled }
		);
    }

	currentFrame = 0;
	semaphoreIndex = 0;
}

void SyncObjects::cleanup() {
	presentCompleteSemaphores.clear();
	renderFinishedSemaphores.clear();
	inFlightFences.clear();

	currentFrame = 0;
	semaphoreIndex = 0;
}
