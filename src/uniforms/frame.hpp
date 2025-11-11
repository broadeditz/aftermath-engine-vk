#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>

struct FrameUniforms {
    float time;
    float aperture;
    float focusDistance;
    float fov;
};

class FrameDataManager {
public:
    FrameDataManager() = default;
    ~FrameDataManager() = default;

    // Initialize everything
    void create(const vk::raii::Device& device, VmaAllocator allocator);

    // Update time every frame
    void update(FrameUniforms uniforms);

    // Bind descriptor set in command buffer
    void bind(const vk::raii::CommandBuffer& commandBuffer,
        const vk::raii::PipelineLayout& pipelineLayout);

    // Get descriptor set layout for pipeline creation
    const vk::raii::DescriptorSetLayout& getDescriptorSetLayout() const {
        return m_descriptorSetLayout;
    }

    vk::DescriptorSet getDescriptorSet() const {
        return m_descriptorSet;
    }

    // Cleanup
    void destroy();

private:
    VmaAllocator m_allocator = VK_NULL_HANDLE;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    void* m_mappedData = nullptr;

    vk::raii::DescriptorSetLayout m_descriptorSetLayout = nullptr;
    vk::raii::DescriptorPool m_descriptorPool = nullptr;
    vk::DescriptorSet m_descriptorSet = VK_NULL_HANDLE;
};
