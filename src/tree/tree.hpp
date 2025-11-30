#ifndef TREE_HPP
#define TREE_HPP

#include <vma/vk_mem_alloc.h>
#include <cstdint>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <iostream>
#include <iomanip>
#include <string>
#include <map>
#include <unordered_map>
#include <thread>
#include <unordered_set>
#include "buffer.hpp"
#include "../util/channel.hpp"
#include "../util/waitgroup.hpp"

const uint32_t LEAF_NODE_FLAG = 0x80000000;
const int treeDepth = 9;
const float baseVoxelSize = 0.25f;

struct vec3 {
    float x, y, z;
};

inline float length(vec3 pos) {
    return sqrt(pos.x * pos.x + pos.y * pos.y + pos.z * pos.z);
}

inline vec3 sub(vec3 pos, vec3 move) {
    return { pos.x - move.x, pos.y - move.y, pos.z - move.z };
}

struct nodeToProcess {
    uint32_t parentNodeIndex;
    int depth;
    vec3 parentPosition;
};

vec3 getChunkPosition(uint32_t chunkIndex, float voxelSize, vec3 parentPosition);
int calculateLOD(int treeDepth, float distance, float lengthThreshold);
float sampleDistanceAt(vec3 position);

class TreeManager {
public:
    std::vector<TreeNode> nodes;
    std::deque<uint32_t> freeNodeIndices;
    std::vector<TreeLeaf> leaves;
    std::deque<uint32_t> freeLeafIndices;

    // GPU buffers
    TreeNodeBuffer nodeBuffer;
    TreeLeafBuffer leafBuffer;

    // Initialize buffers with VMA allocator
    void initBuffers(VmaAllocator allocator, VkDevice device, uint32_t queueFamilyIndex) {
        nodeBuffer.init(allocator, device, queueFamilyIndex);
        leafBuffer.init(allocator, device, queueFamilyIndex);
    }

    // Upload current CPU data to GPU
    void uploadToGPU() {
        nodeBuffer.create(nodes);
        leafBuffer.create(leaves);
    }

    // Update GPU buffers after modifications
    void updateGPUBuffers() {
        nodeBuffer.update(nodes);
        leafBuffer.update(leaves);
    }

    // Get buffers for binding to descriptors
    VkBuffer getNodeBuffer() const { return nodeBuffer.getBuffer(); }
    VkBuffer getLeafBuffer() const { return leafBuffer.getBuffer(); }

    // Cleanup
    void destroyBuffers() {
        nodeBuffer.destroy();
        leafBuffer.destroy();
    }

    void createTestTree();
    void moveObserver(vec3 pos);
    void updateStaleLODs();

    // Destructor to clean up workers
    ~TreeManager() {
        stopWorkers();
        destroyBuffers();
    }

private:
    vec3 observerPos;
    vec3 rootPosition = {
        .x = 0.0,
        .y = 0.0,
        .z = 0.0,
    };

    // Work queue
    std::vector<std::thread> workers;
    Channel<nodeToProcess> queue;
    WaitGroup wg;

    // Synchronization primitives
    std::shared_mutex nodesMutex;      // Protects nodes vector
    std::mutex leavesMutex;     // Protects leaves vector

    // Stale node tracking
    Channel<nodeToProcess> staleQueue;  // Queue of stale nodes to rebuild
    float lastObserverUpdateDistance = 0.0f;  // Track significant movement

    // Voxel sizes
    std::vector<float> voxelSizesAtDepth;

    // Thread-safe operations
    uint32_t createLeaf(float distance, bool lod);
    void subdivideNode(uint32_t parentIndex, int parentDepth, vec3 parentPosition);
    void workerThread();

    // TODO: store freed indices for reuse
    void printTreeStats();
    void printTree(uint32_t nodeIndex = 0, int depth = 0, std::string prefix = "", bool isLast = true);
    void visualizeTreeSlice(int sliceZ = 0);
    void printLeafDistribution();
    void countNodesAtDepth(uint32_t nodeIndex, int depth,
        std::vector<int>& nodesPerLevel,
        std::vector<int>& leavesPerLevel);

    float getVoxelSizeAtDepth(int depth);

    void initVoxelSizes() {
        voxelSizesAtDepth.resize(treeDepth + 1);
        for (int i = 0; i <= treeDepth; i++) {
            voxelSizesAtDepth[i] = baseVoxelSize * pow(4.0f, treeDepth - i);
        }
    }

    void startWorkers() {
        unsigned int numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;

        workers.reserve(numThreads);
        for (unsigned int i = 0; i < numThreads; i++) {
            workers.emplace_back(&TreeManager::workerThread, this);
        }

        std::cout << "Starting tree generation with " << numThreads << " threads..." << std::endl;
    }

    void stopWorkers() {
        queue.close();
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers.clear();
    }

    void markStaleNode(nodeToProcess node);
    void markStaleRecursive(uint32_t nodeIndex, int depth, vec3 nodePosition);

    // Thread-safe node allocation - reserves space for 64 children at once,
    // to ensure somewhat contiguous memory allocation. Reuse freed nodes if available.
    uint32_t allocateChildNodes() {
	    if (!freeNodeIndices.empty()) {
	        uint32_t index = std::move(freeNodeIndices.front());
	        freeNodeIndices.pop_front();
	        return index;
	    }

        std::unique_lock<std::shared_mutex> lock(nodesMutex);
        uint32_t childPointer = nodes.size();

        // Reserve space for all 64 children
        nodes.resize(nodes.size() + 64);

        return childPointer;
    }

    // Free a node and all its descendants
    void freeNode(uint32_t index, bool pushBack) {
        if (index >= nodes.size()) return;

        uint32_t childPointer = nodes[index].childPointer;

        if (childPointer & LEAF_NODE_FLAG) {
            // Free the leaf
            uint32_t leafIndex = childPointer & ~LEAF_NODE_FLAG;
            freeLeaf(leafIndex);
        } else if (childPointer != 0) {
            // Recursively free all 64 children
            for (uint32_t i = 0; i < 64; i++) {
                freeNode(childPointer + i, i == 0);
            }
        }

        // Clear and mark this node as free
        nodes[index] = TreeNode{};
        if (pushBack) {
            freeNodeIndices.push_back(index);
        }
    }

    // Same pattern for leaves
    uint32_t allocateLeaf() {
        if (!freeLeafIndices.empty()) {
            uint32_t index = std::move(freeLeafIndices.front());
            freeLeafIndices.pop_front();
            return index;
        }

        uint32_t index = leaves.size();
        leaves.push_back(TreeLeaf{});
        return index;
    }

    void freeLeaf(uint32_t index) {
        leaves[index] = TreeLeaf{};
        freeLeafIndices.push_back(index);
    }
};

#endif // TREE_HPP
