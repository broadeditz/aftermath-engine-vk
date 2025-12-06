#include "tree.hpp"
#include "buffer.hpp"

#include <cmath>
#include <glm/glm.hpp>
#include <shared_mutex>
#include <thread>
#include <mutex>

// 3D Hash function for noise
float hash3D(glm::vec3 p) {
    p = glm::fract(p * glm::vec3(443.897f, 441.423f, 437.195f));
    p += glm::dot(p, glm::vec3(p.y, p.z, p.x) + 19.19f);
    return glm::fract((p.x + p.y) * p.z) * 2.0f - 1.0f; // Return [-1, 1]
}

// 3D Perlin-style noise
float noise3D(glm::vec3 p) {
    glm::vec3 i = glm::floor(p);
    glm::vec3 f = glm::fract(p);
    f = f * f * (3.0f - 2.0f * f); // Smoothstep

    // Sample all 8 corners of the cube
    float n000 = hash3D(i + glm::vec3(0, 0, 0));
    float n100 = hash3D(i + glm::vec3(1, 0, 0));
    float n010 = hash3D(i + glm::vec3(0, 1, 0));
    float n110 = hash3D(i + glm::vec3(1, 1, 0));
    float n001 = hash3D(i + glm::vec3(0, 0, 1));
    float n101 = hash3D(i + glm::vec3(1, 0, 1));
    float n011 = hash3D(i + glm::vec3(0, 1, 1));
    float n111 = hash3D(i + glm::vec3(1, 1, 1));

    // Trilinear interpolation
    return glm::mix(
        glm::mix(glm::mix(n000, n100, f.x), glm::mix(n010, n110, f.x), f.y),
        glm::mix(glm::mix(n001, n101, f.x), glm::mix(n011, n111, f.x), f.y),
        f.z
    );
}

// 3D FBM
float fbm3D(glm::vec3 p, int octaves = 4, float lacunarity = 2.0f, float gain = 0.5f) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; i++) {
        value += amplitude * noise3D(p * frequency);
        maxValue += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }

    return value / maxValue; // Returns [-1, 1]
}

// Volumetric terrain SDF - fully ray-marchable
float terrainSDF(glm::vec3 p) {
    // Base ground plane at y=0
    float groundLevel = -3.0f;

    // Parameters
    const float scale = 0.01f;          // Noise frequency
    const float amplitude = 30.0f;      // Height variation
    const float density = 0.3f;         // How "solid" the noise is

    // Large-scale terrain height using 2D noise
    glm::vec3 heightSample = glm::vec3(p.x, 0.0f, p.z) * scale;
    float terrainHeight = groundLevel + fbm3D(heightSample, 4) * amplitude;

    // 3D volumetric noise for caves/overhangs/details
    glm::vec3 volumeSample = p * scale * 2.0f;
    float volumeNoise = fbm3D(volumeSample, 3);

    // Combine: start with height field, then add volumetric details
    float heightSDF = p.y - terrainHeight;

    // Add 3D noise that gets stronger underground
    // This creates caves, overhangs, etc.
    float depthFactor = glm::clamp(-heightSDF / 50.0f, 0.0f, 1.0f);
    float volumeContribution = volumeNoise * 10.0f * depthFactor;

    return heightSDF + volumeContribution - density;
}

float sampleDistanceAt(vec3 position) {
    // Simple test example: distance increases with height, flat floor at -2 y
    //return position.y + 2;

    // simple sphere
    //return length(sub(position, vec3{0, 0, 10})) - 5;

    return terrainSDF(glm::vec3{ position.x, position.y, position.z });
}

float TreeManager::getVoxelSizeAtDepth(int depth) {
    return voxelSizesAtDepth[depth];
}

// Function to compute chunk center position based on sexagintaquartant index within the known parent chunk.
// Chunk indexing is as follows:
// Index 0-3 row 0 of layer 0
// Index 4-7 row 1 of layer 0
// ...
vec3 getChunkPosition2(uint32_t chunkIndex, float voxelSize, vec3 parentPosition) {
    // Validate input
    if (chunkIndex >= 64) {
        // Return parent position for invalid index
        return parentPosition;
    }

    // Extract 3D coordinates from the linear index
    // The subdivision is 4×4×4, so we have:
    // - 4 chunks in X direction (index % 4)
    // - 4 chunks in Y direction ((index / 4) % 4)
    // - 4 chunks in Z direction (index / 16)
    uint32_t x = chunkIndex % 4;
    uint32_t y = (chunkIndex / 4) % 4;
    uint32_t z = chunkIndex / 16;

    // Calculate the offset from parent center
    // Parent chunk spans 4 child chunks in each direction
    // Child chunks are arranged from -1.5 to +1.5 in relative coordinates
    // (positions: -1.5, -0.5, +0.5, +1.5 in units of voxelSize)
    float offsetX = (x - 1.5f) * voxelSize;
    float offsetY = (y - 1.5f) * voxelSize;
    float offsetZ = (z - 1.5f) * voxelSize;

    // Return the child chunk center position
    vec3 result;
    result.x = parentPosition.x + offsetX;
    result.y = parentPosition.y + offsetY;
    result.z = parentPosition.z + offsetZ;

    return result;
}

// Precompute offset lookup table
static const float offsets[4] = { -1.5f, -0.5f, 0.5f, 1.5f };

vec3 getChunkPosition(uint32_t chunkIndex, float voxelSize, vec3 parentPosition) {
    uint32_t x = chunkIndex & 3;        // % 4
    uint32_t y = (chunkIndex >> 2) & 3; // / 4 % 4
    uint32_t z = chunkIndex >> 4;       // / 16

    return {
        parentPosition.x + offsets[x] * voxelSize,
        parentPosition.y + offsets[y] * voxelSize,
        parentPosition.z + offsets[z] * voxelSize
    };
}

// TODO: precompute
int calculateLOD(int treeDepth, float distance, float lengthThreshold) {
    int LOD = treeDepth;

    // Reduce LOD by 1 for each doubling of lengthThreshold
    if (distance > lengthThreshold) {
        int reductionSteps = static_cast<int>(sqrt(distance / lengthThreshold));
        LOD -= reductionSteps;
    }

    // clamp LOD to prevent it from going below a certain value
    return (LOD > 3) ? LOD : 3;
}

const float minStep = 0.33;

float sampleLipschitzBoundAt(vec3 position, float voxelSize) {
    float centerDistance = sampleDistanceAt(position);

    // Conservative bound on magnitude
    float halfDiagonal = voxelSize * 1.732050808f * 0.5f;
    //float halfDiagonal = voxelSize * 0.5f;
    float conservativeMagnitude = abs(centerDistance) - halfDiagonal;

    // Preserve the sign from center sample.
    // The minStep is kind of a magic number here that I'm not entirely sure why it makes the voxel marching work just right.
    // I'm also not sure if it's the optimal value at 0.05
    return (centerDistance >= 0 ? 1.0f : -1.0f) * fmax(conservativeMagnitude, voxelSize * minStep * 0.01);
}

float getLipschitzBound(float centerDistance, float voxelSize) {
    // Conservative bound on magnitude
    float halfDiagonal = voxelSize * 1.732050808f * 0.5f;
    //float halfDiagonal = voxelSize * 0.5f;
    float conservativeMagnitude = abs(centerDistance) - halfDiagonal;

    // Preserve the sign from center sample.
    // The minStep is kind of a magic number here that I'm not entirely sure why it makes the voxel marching work just right.
    // I'm also not sure if it's the optimal value at 0.05
    return (centerDistance >= 0 ? 1.0f : -1.0f) * fmax(conservativeMagnitude, voxelSize * minStep * 0.01);
}

// TODO: update name & fingerprint to reflect the fact that this now is only supposed to be used for sparsity leaves
uint32_t TreeManager::createLeaf(float distance) {
    MaterialType material = MaterialType::Void;
    if (distance < 0.00) {
        material = MaterialType::Grass;
    }

    uint8_t flags = 0;

    TreeLeaf leaf = {
        .distance = distance,
        .material = material,
        .damage = 0,
        .flags = flags,
    };

    std::lock_guard<std::mutex> lock(leavesMutex);
    uint32_t leafPointer = leaves.size();
    leaves.push_back(leaf);

    return leafPointer;
}

void TreeManager::createLeaves(uint32_t parentIndex, int depth, vec3 parentPosition) {
	float voxelSize = getVoxelSizeAtDepth(depth);

	uint64_t childMask = 0;

	std::vector<TreeLeaf> newLeaves;
	newLeaves.reserve(64);
	for (uint32_t i = 0; i < 64; i++) {
        vec3 childPosition = getChunkPosition(i, voxelSize, parentPosition);
        float distance = sampleDistanceAt(childPosition);
        distance = getLipschitzBound(distance, voxelSize);
        if (abs(distance) < voxelSize * minStep) {
            distance = (distance >= 0 ? 1.0f : -1.0f) * voxelSize * minStep;
        }

        if (distance < 0) {
            childMask |= (1ULL << i);
        }

        TreeLeaf leaf = {
            .distance = distance,
            .material = distance < 0 ? MaterialType::Grass : MaterialType::Void,
            .damage = 0,
            .flags = 1,
        };

        newLeaves.push_back(leaf);
    }

	uint32_t leafPointer = 0;
	{
		std::lock_guard<std::mutex> lock(leavesMutex);
		leafPointer = leaves.size();
		leaves.insert(leaves.end(), newLeaves.begin(), newLeaves.end());
	}

	std::shared_lock<std::shared_mutex> lock(nodesMutex);
	nodes[parentIndex].childPointer = leafPointer | LEAF_NODE_FLAG;
	nodes[parentIndex].childMask = childMask;
}

// Create children for a node and add them to the processing queue
void TreeManager::subdivideNode(
    uint32_t parentIndex,
    int depth,
    vec3 parentPosition
) {
    //std::cout << depth << std::endl;
    float voxelSize = getVoxelSizeAtDepth(depth);

    float distance = sampleDistanceAt(parentPosition);

    // create sparsity leaf if the nearest surface is further than the size of the node
    float halfDiagonal = voxelSize * 1.732050808f * 0.5f;
    if (abs(distance) > halfDiagonal * 1.01) {
        uint32_t leafPointer = createLeaf(getLipschitzBound(distance, voxelSize));

        {
            std::shared_lock<std::shared_mutex> lock(nodesMutex);
            nodes[parentIndex].childPointer = leafPointer | LEAF_NODE_FLAG;
        }

        return;
    }

    // Use observer position for LOD calculation
    float distanceFromCamera = length(sub(parentPosition, observerPos));
    int LOD = calculateLOD(treeDepth, distanceFromCamera, 128);

    // create voxel leaf if at smallest possible voxel resolution
    if (depth >= LOD - 1) {
        createLeaves(parentIndex, depth + 1, parentPosition);

        return;
    }

    // Allocate space for all 64 children at once (thread-safe)
    uint32_t childPointer = allocateChildNodes();
    {
        std::shared_lock<std::shared_mutex> nodesLock(nodesMutex);
        nodes[parentIndex].childPointer = childPointer;
    }

    voxelSize = getVoxelSizeAtDepth(depth + 1);

    // Create 64 children (4×4×4 subdivision)

    std::vector<nodeToProcess> newNodes;
    newNodes.reserve(64);
    for (uint32_t i = 0; i < 64; i++) {
        vec3 childPosition = getChunkPosition(i, voxelSize, parentPosition);

        TreeNode childNode = {};
        childNode.childPointer = 0;

        uint32_t childIndex = childPointer + i;

        {
            std::shared_lock<std::shared_mutex> nodesLock(nodesMutex);
            nodes[childIndex] = childNode;
        }

        // Add child to queue for further processing (thread-safe)
        newNodes.push_back(nodeToProcess{ childIndex, depth + 1, childPosition });
    }

    wg.add(64);
    queue.sendMany(newNodes);
}

// Worker thread function
void TreeManager::workerThread() {
    while (true) {
        nodeToProcess current;

        // Try to get work from queue
        {
            if (!queue.receive(current)) {
            	break; // channel closed
            }
        }

        // If we got work, process it
        subdivideNode(current.parentNodeIndex, current.depth, current.parentPosition);
        wg.done();
    }

    std::this_thread::yield();
}

void TreeManager::createTestTree() {
    initVoxelSizes();

    nodes.clear();
    leaves.clear();

    // Initialize observer at origin
    observerPos = {0.0f, 0.0f, 0.0f};

    // Simple test tree with one root and 8 leaf children
    TreeNode rootNode = {};
    rootNode.childPointer = 1; // First child at index 1
    nodes.push_back(rootNode);

    uint32_t leafIndex = 0;
    float voxelSize = getVoxelSizeAtDepth(1);

    // Reserve space for all 64 root children first
    uint32_t firstChildIndex = nodes.size();
    for (uint32_t i = 0; i < 64; i++) {
        TreeNode childNode = {};
        childNode.childPointer = 0;
        nodes.push_back(childNode);
    }

    // Create 64 children (4×4×4 subdivision) and add to queue
    for (uint32_t i = 0; i < 64; i++) {
        vec3 childPosition = getChunkPosition(i, voxelSize, rootPosition);
        uint32_t childIndex = firstChildIndex + i;
        queue.send(nodeToProcess{ childIndex, 1, childPosition });
    }

    // Initialize active worker counter
    wg.add(64);

    startWorkers();

    // Wait for all workers to complete
    wg.wait();

    std::cout << "Tree generation complete!" << std::endl;

    queue.shrink_to_fit();

    printTreeStats();
    visualizeTreeSlice();
}
