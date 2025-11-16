#include <vk_mem_alloc.h>
#include <cstdint>
#include <vector>
#include <deque>
#include "buffer.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <map>

const uint32_t LEAF_NODE_FLAG = 0x80000000;

const int treeDepth = 9; // Example depth for test tree
const float baseVoxelSize = 0.25f; // Size of the smallest voxel at the deepest level

struct vec3 {
    float x, y, z;
};

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
	std::vector<TreeLeaf> leaves;

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
private:
    std::deque<nodeToProcess> queue;
    std::vector<float> voxelSizesAtDepth;

    uint32_t createLeaf(float distance);
    void subdivideNode(uint32_t parentIndex, int parentDepth, vec3 parentPosition);
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
};