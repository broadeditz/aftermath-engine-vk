#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>

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
};

struct TreeLeaf {
    float distance; // Distance from block center to nearest surface, can be negative
    MaterialType material;
    uint8_t damage; // damage to the block, 0-255
    uint8_t padding[2]; // Padding for alignment, not needed, but good to be reminded that we can use another 16 bits worth of data here if needed
};

template<typename T>
class TreeBuffer {
public:
    TreeBuffer() = default;
    ~TreeBuffer() = default;

    void init(VmaAllocator allocator) {
        m_allocator = allocator;
    }

    bool create(const std::vector<T>& initialData) {
        if (!m_allocator || initialData.empty()) {
            return false;
        }

        m_count = initialData.size();
        m_capacity = m_count;
        VkDeviceSize bufferSize = m_capacity * sizeof(T);

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VkResult result = vmaCreateBuffer(
            m_allocator,
            &bufferInfo,
            &allocInfo,
            &m_buffer,
            &m_allocation,
            &m_allocationInfo
        );

        if (result != VK_SUCCESS) {
            return false;
        }

        std::memcpy(m_allocationInfo.pMappedData, initialData.data(), bufferSize);
        flush(0, bufferSize);
        return true;
    }

    bool createEmpty(size_t capacity) {
        if (!m_allocator || capacity == 0) {
            return false;
        }

        m_count = 0;
        m_capacity = capacity;
        VkDeviceSize bufferSize = m_capacity * sizeof(T);

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VkResult result = vmaCreateBuffer(
            m_allocator,
            &bufferInfo,
            &allocInfo,
            &m_buffer,
            &m_allocation,
            &m_allocationInfo
        );

        if (result != VK_SUCCESS) {
            return false;
        }

        std::memset(m_allocationInfo.pMappedData, 0, bufferSize);
        flush(0, bufferSize);
        return true;
    }

    // resize creates a new vulkan buffer with given capacity, and copies the existing buffer into it
    bool resize(size_t newCapacity) {
        if (newCapacity <= m_capacity) {
            return true; // Already big enough
        }

        // Save old data if it exists
        std::vector<T> oldData;
        if (m_buffer != VK_NULL_HANDLE && m_count > 0) {
            oldData.resize(m_count);
            std::memcpy(oldData.data(), m_allocationInfo.pMappedData, m_count * sizeof(T));
            destroy();
        }

        // Create new larger buffer
        if (!createEmpty(newCapacity)) {
            return false;
        }

        // Copy old data back if we had any
        if (!oldData.empty()) {
            std::memcpy(m_allocationInfo.pMappedData, oldData.data(), oldData.size() * sizeof(T));
            flush(0, oldData.size() * sizeof(T));
            m_count = oldData.size();
        }

        return true;
    }

    void update(const std::vector<T>& data) {
        if (!m_buffer || data.empty() || data.size() > m_capacity) {
            return;
        }

        m_count = data.size();
        VkDeviceSize size = m_count * sizeof(T);
        std::memcpy(m_allocationInfo.pMappedData, data.data(), size);
        flush(0, size);
    }

    void updateRange(uint32_t startIndex, uint32_t count, const T* elements) {
        if (!m_buffer || !elements || count == 0 || startIndex + count > m_capacity) {
            return;
        }

        VkDeviceSize offset = startIndex * sizeof(T);
        VkDeviceSize size = count * sizeof(T);

        uint8_t* dst = static_cast<uint8_t*>(m_allocationInfo.pMappedData) + offset;
        std::memcpy(dst, elements, size);
        flush(offset, size);

        m_count = std::max(m_count, static_cast<size_t>(startIndex + count));
    }

    void updateElement(uint32_t index, const T& element) {
        if (!m_buffer || index >= m_capacity) {
            return;
        }

        T* elements = static_cast<T*>(m_allocationInfo.pMappedData);
        elements[index] = element;

        VkDeviceSize offset = index * sizeof(T);
        flush(offset, sizeof(T));

        m_count = std::max(m_count, static_cast<size_t>(index + 1));
    }

    VkBuffer getBuffer() const { return m_buffer; }
    size_t getCount() const { return m_count; }
    size_t getCapacity() const { return m_capacity; }

    void destroy() {
        if (m_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
            m_buffer = VK_NULL_HANDLE;
            m_allocation = VK_NULL_HANDLE;
        }
        m_count = 0;
        m_capacity = 0;
    }

private:
    VmaAllocator m_allocator = nullptr;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VmaAllocationInfo m_allocationInfo = {};

    size_t m_count = 0;
    size_t m_capacity = 0;

    void flush(VkDeviceSize offset, VkDeviceSize size) {
        if (m_allocation != VK_NULL_HANDLE) {
            vmaFlushAllocation(m_allocator, m_allocation, offset, size);
        }
    }
};

// Type aliases
using TreeNodeBuffer = TreeBuffer<TreeNode>;
using TreeLeafBuffer = TreeBuffer<TreeLeaf>;