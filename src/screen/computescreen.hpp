#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>
#include "../uniforms/frame.hpp"

class ComputeToScreen {
public:
    vk::Image image;  // Keep raw since VMA manages this
    VmaAllocation allocation;
    vk::raii::ImageView view = nullptr;
    vk::raii::Sampler sampler = nullptr;
    vk::raii::DescriptorSetLayout computeLayout = nullptr;
    vk::raii::DescriptorSetLayout graphicsLayout = nullptr;
	vk::raii::DescriptorSets computeSets = nullptr;
	vk::raii::DescriptorSets graphicsSets = nullptr;
    std::array<vk::DescriptorSet, 2> descriptorSets;
    vk::raii::DescriptorPool pool = nullptr;
    vk::DescriptorSet computeSet;  // Keep raw - owned by pool
    vk::DescriptorSet graphicsSet; // Keep raw - owned by pool
    vk::raii::PipelineLayout computePipelineLayout = nullptr;
    vk::raii::PipelineLayout graphicsPipelineLayout = nullptr;
    FrameDataManager frameData;

    uint32_t width, height;

    void create(VmaAllocator allocator, const vk::raii::Device& device, uint32_t width, uint32_t height);
    void destroy(VmaAllocator allocator);

    // Helper to transition image for first use
    void initialTransition(const vk::raii::CommandBuffer& cmd);

    // Record compute dispatch and barrier
    void recordCompute(const vk::raii::CommandBuffer& cmd, const vk::raii::Pipeline& computePipeline);

    // Record graphics draw (call inside render pass)
    void recordGraphics(const vk::raii::CommandBuffer& cmd, const vk::raii::Pipeline& graphicsPipeline);

    // Transition back to GENERAL for next frame
    void transitionBack(const vk::raii::CommandBuffer& cmd);
};