#pragma once
#include <array>
#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <stdexcept>

enum class MaterialType : uint8_t {
    Void = 0,
    Air = 1,
    Water = 2,
    Dirt = 3,
    Stone = 4,
    Grass = 5,
    Sand = 6,
    Wood = 7,
    Leaf = 8,
    Glass = 9,
    Torch = 10,
};

// Tree node structure - must match shader definition
struct TreeNode {
    uint32_t childPointer; // Pointer to first child, if LEAF_NODE_FLAG is set, it means it's a leaf node, and contains the index for the leaf data.
    uint8_t flags;
    std::array<uint8_t,3> padding;
};

struct TreeLeaf {
    float distance; // Distance from block center to nearest surface, can be negative if the block is inside of the geometry.
    MaterialType material;
    uint8_t damage; // damage to the block, 0-255
    uint8_t flags;
    uint8_t padding; // 8 bits of padding left
};

template<typename T>
class TreeBuffer {
public:
    TreeBuffer() = default;
    ~TreeBuffer() = default;

    void init(VmaAllocator allocator, VkDevice device, uint32_t queueFamilyIndex) {
        m_allocator = allocator;
        m_device = device;
        m_queueFamilyIndex = queueFamilyIndex;

        // Create our own command pool
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT; // Optimized for short-lived command buffers
        poolInfo.queueFamilyIndex = queueFamilyIndex;

        if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create TreeBuffer command pool!");
        }

        // Get our own queue handle
        vkGetDeviceQueue(m_device, queueFamilyIndex, 0, &m_queue);
    }

    bool create(const std::vector<T>& initialData) {
        if (!m_allocator || initialData.empty()) {
            return false;
        }

        m_count = initialData.size();
        m_capacity = m_count;
        VkDeviceSize bufferSize = m_capacity * sizeof(T);

        // Create GPU buffer
        if (!createGPUBuffer(bufferSize)) {
            return false;
        }

        // Create staging buffer
        if (!createStagingBuffer(bufferSize)) {
            destroyGPUBuffer();
            return false;
        }

        // Copy data to staging buffer
        std::memcpy(m_stagingAllocationInfo.pMappedData, initialData.data(), bufferSize);
        vmaFlushAllocation(m_allocator, m_stagingAllocation, 0, bufferSize);

        // Copy from staging to GPU
        if (!copyToGPU(0, bufferSize)) {
            destroy();
            return false;
        }

        return true;
    }

    bool createEmpty(size_t capacity) {
        if (!m_allocator || capacity == 0) {
            return false;
        }

        m_count = 0;
        m_capacity = capacity;
        VkDeviceSize bufferSize = m_capacity * sizeof(T);

        // Create GPU buffer
        if (!createGPUBuffer(bufferSize)) {
            return false;
        }

        // Create staging buffer
        if (!createStagingBuffer(bufferSize)) {
            destroyGPUBuffer();
            return false;
        }

        // Zero out staging buffer
        std::memset(m_stagingAllocationInfo.pMappedData, 0, bufferSize);
        vmaFlushAllocation(m_allocator, m_stagingAllocation, 0, bufferSize);

        // Copy zeros to GPU
        if (!copyToGPU(0, bufferSize)) {
            destroy();
            return false;
        }

        return true;
    }

    bool resize(size_t newCapacity) {
        if (newCapacity <= m_capacity) {
            return true; // Already big enough
        }

        // Create temporary buffers for the new size
        VkBuffer newGPUBuffer = VK_NULL_HANDLE;
        VmaAllocation newGPUAllocation = VK_NULL_HANDLE;
        VkBuffer newStagingBuffer = VK_NULL_HANDLE;
        VmaAllocation newStagingAllocation = VK_NULL_HANDLE;
        VmaAllocationInfo newStagingInfo = {};

        VkDeviceSize newBufferSize = newCapacity * sizeof(T);

        // Create new GPU buffer
        {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = newBufferSize;
            bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

            VkResult result = vmaCreateBuffer(
                m_allocator,
                &bufferInfo,
                &allocInfo,
                &newGPUBuffer,
                &newGPUAllocation,
                nullptr
            );

            if (result != VK_SUCCESS) {
                return false;
            }
        }

        // Create new staging buffer
        {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = newBufferSize;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT;

            VkResult result = vmaCreateBuffer(
                m_allocator,
                &bufferInfo,
                &allocInfo,
                &newStagingBuffer,
                &newStagingAllocation,
                &newStagingInfo
            );

            if (result != VK_SUCCESS) {
                vmaDestroyBuffer(m_allocator, newGPUBuffer, newGPUAllocation);
                return false;
            }
        }

        // Copy old data if it exists
        if (m_gpuBuffer != VK_NULL_HANDLE && m_count > 0) {
            VkDeviceSize oldSize = m_count * sizeof(T);

            // Copy from old GPU buffer to new GPU buffer
            if (!copyBufferToBuffer(m_gpuBuffer, newGPUBuffer, oldSize)) {
                vmaDestroyBuffer(m_allocator, newGPUBuffer, newGPUAllocation);
                vmaDestroyBuffer(m_allocator, newStagingBuffer, newStagingAllocation);
                return false;
            }
        }

        // Destroy old buffers
        destroyBuffers();

        // Assign new buffers
        m_gpuBuffer = newGPUBuffer;
        m_gpuAllocation = newGPUAllocation;
        m_stagingBuffer = newStagingBuffer;
        m_stagingAllocation = newStagingAllocation;
        m_stagingAllocationInfo = newStagingInfo;
        m_capacity = newCapacity;

        return true;
    }

    void update(const std::vector<T>& data) {
        if (!m_gpuBuffer || data.empty() || data.size() > m_capacity) {
            return;
        }

        m_count = data.size();
        VkDeviceSize size = m_count * sizeof(T);
        std::memcpy(m_stagingAllocationInfo.pMappedData, data.data(), size);
        vmaFlushAllocation(m_allocator, m_stagingAllocation, 0, size);
        copyToGPU(0, size);
    }

    void updateRange(uint32_t startIndex, uint32_t count, const T* elements) {
        if (!m_gpuBuffer || !elements || count == 0 || startIndex + count > m_capacity) {
            return;
        }

        VkDeviceSize offset = startIndex * sizeof(T);
        VkDeviceSize size = count * sizeof(T);

        uint8_t* dst = static_cast<uint8_t*>(m_stagingAllocationInfo.pMappedData) + offset;
        std::memcpy(dst, elements, size);
        vmaFlushAllocation(m_allocator, m_stagingAllocation, offset, size);
        copyToGPU(offset, size);

        m_count = std::max(m_count, static_cast<size_t>(startIndex + count));
    }

    void updateElement(uint32_t index, const T& element) {
        if (!m_gpuBuffer || index >= m_capacity) {
            return;
        }

        T* elements = static_cast<T*>(m_stagingAllocationInfo.pMappedData);
        elements[index] = element;

        VkDeviceSize offset = index * sizeof(T);
        vmaFlushAllocation(m_allocator, m_stagingAllocation, offset, sizeof(T));
        copyToGPU(offset, sizeof(T));

        m_count = std::max(m_count, static_cast<size_t>(index + 1));
    }

    VkBuffer getBuffer() const { return m_gpuBuffer; }
    size_t getCount() const { return m_count; }
    size_t getCapacity() const { return m_capacity; }

    void destroy() {
        destroyBuffers();

        if (m_commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device, m_commandPool, nullptr);
            m_commandPool = VK_NULL_HANDLE;
        }

        m_queue = VK_NULL_HANDLE;
        m_count = 0;
        m_capacity = 0;
    }

private:
    VmaAllocator m_allocator = nullptr;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    uint32_t m_queueFamilyIndex = 0;

    VkBuffer m_gpuBuffer = VK_NULL_HANDLE;
    VmaAllocation m_gpuAllocation = VK_NULL_HANDLE;

    VkBuffer m_stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation m_stagingAllocation = VK_NULL_HANDLE;
    VmaAllocationInfo m_stagingAllocationInfo = {};

    size_t m_count = 0;
    size_t m_capacity = 0;

    bool createGPUBuffer(VkDeviceSize size) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        VkResult result = vmaCreateBuffer(
            m_allocator,
            &bufferInfo,
            &allocInfo,
            &m_gpuBuffer,
            &m_gpuAllocation,
            nullptr
        );

        return result == VK_SUCCESS;
    }

    bool createStagingBuffer(VkDeviceSize size) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VkResult result = vmaCreateBuffer(
            m_allocator,
            &bufferInfo,
            &allocInfo,
            &m_stagingBuffer,
            &m_stagingAllocation,
            &m_stagingAllocationInfo
        );

        return result == VK_SUCCESS;
    }

    bool copyToGPU(VkDeviceSize offset, VkDeviceSize size) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = offset;
        copyRegion.dstOffset = offset;
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, m_stagingBuffer, m_gpuBuffer, 1, &copyRegion);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_queue);

        vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);

        return true;
    }

    bool copyBufferToBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_queue);

        vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);

        return true;
    }

    void destroyGPUBuffer() {
        if (m_gpuBuffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(m_allocator, m_gpuBuffer, m_gpuAllocation);
            m_gpuBuffer = VK_NULL_HANDLE;
            m_gpuAllocation = VK_NULL_HANDLE;
        }
    }

    void destroyStagingBuffer() {
        if (m_stagingBuffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(m_allocator, m_stagingBuffer, m_stagingAllocation);
            m_stagingBuffer = VK_NULL_HANDLE;
            m_stagingAllocation = VK_NULL_HANDLE;
        }
    }

    void destroyBuffers() {
        destroyGPUBuffer();
        destroyStagingBuffer();
    }
};

// Type aliases
using TreeNodeBuffer = TreeBuffer<TreeNode>;
using TreeLeafBuffer = TreeBuffer<TreeLeaf>;
