#include "frame.hpp"
#include <cstring>
#include <stdexcept>

void FrameDataManager::create(const vk::raii::Device& device, VmaAllocator allocator) {
    // Create buffer with VMA
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(FrameUniforms);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo allocationInfo;
    if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
        &m_buffer, &m_allocation, &allocationInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create uniform buffer!");
    }

    m_mappedData = allocationInfo.pMappedData;

    // Create descriptor set layout
    vk::DescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eCompute;

    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    m_descriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);

    // Create descriptor pool
    vk::DescriptorPoolSize poolSize{};
    poolSize.type = vk::DescriptorType::eUniformBuffer;
    poolSize.descriptorCount = 1;

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    m_descriptorPool = device.createDescriptorPool(poolInfo);

    // Allocate descriptor set
    vk::DescriptorSetAllocateInfo allocInfo2{};
    allocInfo2.descriptorPool = *m_descriptorPool;
    allocInfo2.descriptorSetCount = 1;
    allocInfo2.pSetLayouts = &(*m_descriptorSetLayout);

    VkDescriptorSet vkDescSet;
    if (vkAllocateDescriptorSets(*device,
        reinterpret_cast<const VkDescriptorSetAllocateInfo*>(&allocInfo2),
        &vkDescSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate frame descriptor set");
    }
    m_descriptorSet = vk::DescriptorSet(vkDescSet);

    // Update descriptor set
    vk::DescriptorBufferInfo bufferInfo2{};
    bufferInfo2.buffer = m_buffer;
    bufferInfo2.offset = 0;
    bufferInfo2.range = sizeof(FrameUniforms);

    vk::WriteDescriptorSet descriptorWrite{};
    descriptorWrite.dstSet = m_descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo2;

    device.updateDescriptorSets(descriptorWrite, nullptr);
}

void FrameDataManager::update(float time) {
    if (m_mappedData) {
        FrameUniforms uniforms{};
        uniforms.time = time;
        memcpy(m_mappedData, &uniforms, sizeof(FrameUniforms));
    }
}

void FrameDataManager::bind(const vk::raii::CommandBuffer& commandBuffer,
    const vk::raii::PipelineLayout& pipelineLayout) {
    commandBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute,
        *pipelineLayout,
        1,
        m_descriptorSet,
        nullptr
    );
}

void FrameDataManager::destroy() {
	// TODO: Implement cleanup
    /*m_descriptorSet = VK_NULL_HANDLE;
    m_descriptorPool.clear();
    m_descriptorSetLayout.clear();

    if (m_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
        m_buffer = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
        m_mappedData = nullptr;
    }*/
}
