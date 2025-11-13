#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <cstdint>
#include <cstring>

#include "tree.hpp"


class TreeBuffer {
public:
    TreeBuffer() = default;
    ~TreeBuffer() = default;

    // Initialize with allocator
    void init(VmaAllocator allocator);

    // Create buffer with initial data
    bool create(const std::vector<TreeNode>& initialData);

    // Create empty buffer with capacity
    bool createEmpty(size_t nodeCapacity);

    // Update entire buffer
    void update(const std::vector<TreeNode>& data);

    // Update range of nodes
    void updateRange(uint32_t startIndex, uint32_t count, const TreeNode* nodes);

    // Update single node
    void updateNode(uint32_t index, const TreeNode& node);

    // Get buffer for descriptor binding
    VkBuffer getBuffer() const { return m_buffer; }

    // Get current size in nodes
    size_t getNodeCount() const { return m_nodeCount; }

    // Get capacity in nodes
    size_t getCapacity() const { return m_capacity; }

    // Cleanup
    void destroy();

private:
    VmaAllocator m_allocator = nullptr;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VmaAllocationInfo m_allocationInfo = {};

    size_t m_nodeCount = 0;
    size_t m_capacity = 0;

    void flush(VkDeviceSize offset, VkDeviceSize size);
};

// Chunked tree manager for streaming large worlds
class ChunkedTreeBuffer {
public:
    ChunkedTreeBuffer() = default;
    ~ChunkedTreeBuffer() = default;

    struct ChunkInfo {
        uint32_t startIndex;
        uint32_t nodeCount;
        bool occupied;
    };

    // Initialize with max chunks and nodes per chunk
    void init(VmaAllocator allocator, uint32_t maxChunks, uint32_t nodesPerChunk);

    // Load chunk data into specific slot
    bool loadChunk(uint32_t chunkIndex, const std::vector<TreeNode>& chunkData);

    // Unload chunk (marks as unoccupied, optionally clears memory)
    void unloadChunk(uint32_t chunkIndex, bool clearMemory = false);

    // Update nodes within a chunk
    void updateChunkNodes(uint32_t chunkIndex, uint32_t nodeOffset,
        uint32_t count, const TreeNode* nodes);

    // Get chunk info
    const ChunkInfo& getChunkInfo(uint32_t chunkIndex) const;

    // Get buffer for descriptor binding
    VkBuffer getBuffer() const { return m_buffer.getBuffer(); }

    // Get total capacity
    uint32_t getTotalCapacity() const { return m_maxChunks * m_nodesPerChunk; }

    // Cleanup
    void destroy();

private:
    TreeBuffer m_buffer;
    std::vector<ChunkInfo> m_chunks;
    uint32_t m_maxChunks = 0;
    uint32_t m_nodesPerChunk = 0;
};