#include "buffer.hpp"
#include <algorithm>
#include <stdexcept>

// ============================================================================
// OctreeBuffer Implementation
// ============================================================================

void TreeBuffer::init(VmaAllocator allocator) {
    m_allocator = allocator;
}

bool TreeBuffer::create(const std::vector<TreeNode>& initialData) {
    if (!m_allocator) {
        return false;
    }

    if (initialData.empty()) {
        return false;
    }

    m_nodeCount = initialData.size();
    m_capacity = m_nodeCount;

    VkDeviceSize bufferSize = m_capacity * sizeof(TreeNode);

    // Buffer create info
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // VMA allocation info - host-visible, persistently mapped
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT;

    // Create buffer
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

    // Copy initial data
    std::memcpy(m_allocationInfo.pMappedData, initialData.data(), bufferSize);
    flush(0, bufferSize);

    return true;
}

bool TreeBuffer::createEmpty(size_t nodeCapacity) {
    if (!m_allocator) {
        return false;
    }

    if (nodeCapacity == 0) {
        return false;
    }

    m_nodeCount = 0;
    m_capacity = nodeCapacity;

    VkDeviceSize bufferSize = m_capacity * sizeof(TreeNode);

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

    // Zero out buffer
    std::memset(m_allocationInfo.pMappedData, 0, bufferSize);
    flush(0, bufferSize);

    return true;
}

void TreeBuffer::update(const std::vector<TreeNode>& data) {
    if (!m_buffer || data.empty()) {
        return;
    }

    // Check if data fits
    if (data.size() > m_capacity) {
        // Could resize here, but for now just clamp
        return;
    }

    m_nodeCount = data.size();
    VkDeviceSize size = m_nodeCount * sizeof(TreeNode);

    std::memcpy(m_allocationInfo.pMappedData, data.data(), size);
    flush(0, size);
}

void TreeBuffer::updateRange(uint32_t startIndex, uint32_t count, const TreeNode* nodes) {
    if (!m_buffer || !nodes || count == 0) {
        return;
    }

    // Bounds check
    if (startIndex + count > m_capacity) {
        return;
    }

    VkDeviceSize offset = startIndex * sizeof(TreeNode);
    VkDeviceSize size = count * sizeof(TreeNode);

    uint8_t* dst = static_cast<uint8_t*>(m_allocationInfo.pMappedData) + offset;
    std::memcpy(dst, nodes, size);

    flush(offset, size);

    // Update node count if necessary
    m_nodeCount = std::max(m_nodeCount, static_cast<size_t>(startIndex + count));
}

void TreeBuffer::updateNode(uint32_t index, const TreeNode& node) {
    if (!m_buffer) {
        return;
    }

    // Bounds check
    if (index >= m_capacity) {
        return;
    }

    TreeNode* nodes = static_cast<TreeNode*>(m_allocationInfo.pMappedData);
    nodes[index] = node;

    VkDeviceSize offset = index * sizeof(TreeNode);
    flush(offset, sizeof(TreeNode));

    // Update node count if necessary
    m_nodeCount = std::max(m_nodeCount, static_cast<size_t>(index + 1));
}

void TreeBuffer::destroy() {
    if (m_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
        m_buffer = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }

    m_nodeCount = 0;
    m_capacity = 0;
}

void TreeBuffer::flush(VkDeviceSize offset, VkDeviceSize size) {
    if (m_allocation != VK_NULL_HANDLE) {
        vmaFlushAllocation(m_allocator, m_allocation, offset, size);
    }
}

// ============================================================================
// ChunkedTreeBuffer Implementation
// ============================================================================

void ChunkedTreeBuffer::init(VmaAllocator allocator, uint32_t maxChunks, uint32_t nodesPerChunk) {
    m_maxChunks = maxChunks;
    m_nodesPerChunk = nodesPerChunk;

    // Initialize buffer with total capacity
    m_buffer.init(allocator);
    m_buffer.createEmpty(static_cast<size_t>(maxChunks) * nodesPerChunk);

    // Initialize chunk metadata
    m_chunks.reserve(maxChunks);
    for (uint32_t i = 0; i < maxChunks; i++) {
        ChunkInfo info;
        info.startIndex = i * nodesPerChunk;
        info.nodeCount = 0;
        info.occupied = false;
        m_chunks.push_back(info);
    }
}

bool ChunkedTreeBuffer::loadChunk(uint32_t chunkIndex, const std::vector<TreeNode>& chunkData) {
    if (chunkIndex >= m_maxChunks) {
        return false;
    }

    if (chunkData.size() > m_nodesPerChunk) {
        return false; // Chunk data too large
    }

    ChunkInfo& chunk = m_chunks[chunkIndex];

    // Update buffer
    m_buffer.updateRange(chunk.startIndex, chunkData.size(), chunkData.data());

    // Update metadata
    chunk.nodeCount = chunkData.size();
    chunk.occupied = true;

    return true;
}

void ChunkedTreeBuffer::unloadChunk(uint32_t chunkIndex, bool clearMemory) {
    if (chunkIndex >= m_maxChunks) {
        return;
    }

    ChunkInfo& chunk = m_chunks[chunkIndex];

    if (clearMemory && chunk.nodeCount > 0) {
        // Zero out the chunk's memory
        std::vector<TreeNode> zeros(chunk.nodeCount, TreeNode{});
        m_buffer.updateRange(chunk.startIndex, chunk.nodeCount, zeros.data());
    }

    chunk.nodeCount = 0;
    chunk.occupied = false;
}

void ChunkedTreeBuffer::updateChunkNodes(uint32_t chunkIndex, uint32_t nodeOffset,
    uint32_t count, const TreeNode* nodes) {
    if (chunkIndex >= m_maxChunks) {
        return;
    }

    if (nodeOffset + count > m_nodesPerChunk) {
        return; // Out of bounds
    }

    ChunkInfo& chunk = m_chunks[chunkIndex];
    uint32_t absoluteIndex = chunk.startIndex + nodeOffset;

    m_buffer.updateRange(absoluteIndex, count, nodes);

    // Update chunk node count if necessary
    chunk.nodeCount = std::max(chunk.nodeCount, nodeOffset + count);
}

const ChunkedTreeBuffer::ChunkInfo& ChunkedTreeBuffer::getChunkInfo(uint32_t chunkIndex) const {
    static ChunkInfo invalid = { 0, 0, false };

    if (chunkIndex >= m_maxChunks) {
        return invalid;
    }

    return m_chunks[chunkIndex];
}

void ChunkedTreeBuffer::destroy() {
    m_buffer.destroy();
    m_chunks.clear();
    m_maxChunks = 0;
    m_nodesPerChunk = 0;
}