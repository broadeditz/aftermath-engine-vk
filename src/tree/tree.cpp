#include "tree.hpp"
#include "buffer.hpp"

#include <cmath>
#include <glm/glm.hpp>
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

float length(vec3 pos) {
    return sqrt(pos.x * pos.x + pos.y * pos.y + pos.z * pos.z);
}

vec3 sub(vec3 pos, vec3 move) {
    return { pos.x - move.x, pos.y - move.y, pos.z - move.z };
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

uint32_t TreeManager::createLeaf(float distance, bool lod) {
    MaterialType material = MaterialType::Void;
    if (distance < 0.00) {
        material = MaterialType::Grass;
    }

    uint8_t flags = 0;

    if (lod) {
        flags = 1;
    }

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

// Thread-safe node allocation - reserves space for 64 children at once
uint32_t TreeManager::allocateChildNodes() {
    std::lock_guard<std::mutex> lock(nodesMutex);
    uint32_t childPointer = nodes.size();

    // Reserve space for all 64 children
    nodes.resize(nodes.size() + 64);

    return childPointer;
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
    float lipschitz = getLipschitzBound(distance, voxelSize);

    // create sparsity leaf if the nearest surface is further than the size of the node
    float halfDiagonal = voxelSize * 1.732050808f * 0.5f;

    if (abs(lipschitz) > halfDiagonal * 1.01) {
        uint32_t leafPointer = createLeaf(lipschitz, false);

        {
            std::lock_guard<std::mutex> lock(nodesMutex);
            nodes[parentIndex].childPointer = leafPointer | LEAF_NODE_FLAG;
        }

        return;
    }

    // TODO: precompute LOD for distance value as much as possible,
    // also, make it use actual camera position
    float distanceFromCamera = length(parentPosition);
    int LOD = calculateLOD(treeDepth, distanceFromCamera, 128);

    // create voxel leaf if at smallest possible voxel resolution
    if (depth >= LOD) {
        //clamp to prevent steps smaller than voxel size
        if (abs(distance) < voxelSize * minStep) {
            distance = (distance >= 0 ? 1.0f : -1.0f) * voxelSize * minStep;
        }

        uint32_t leafPointer = createLeaf(distance, true);

        {
            std::lock_guard<std::mutex> lock(nodesMutex);
            nodes[parentIndex].childPointer = leafPointer | LEAF_NODE_FLAG;
        }

        return;
    }

    // Allocate space for all 64 children at once (thread-safe)
    uint32_t childPointer = allocateChildNodes();
    {
        std::lock_guard<std::mutex> nodesLock(nodesMutex);
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
            std::lock_guard<std::mutex> nodesLock(nodesMutex);
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

    // Simple test tree with one root and 8 leaf children
    TreeNode rootNode = {};
    rootNode.childPointer = 1; // First child at index 1
    nodes.push_back(rootNode);

    uint32_t leafIndex = 0;
    float voxelSize = getVoxelSizeAtDepth(1);

    vec3 rootPosition = {
        .x = 0.0,
        .y = 0.0,
        .z = 0.0,
    };

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

void TreeManager::printTreeStats() {
    std::cout << "=== Tree Statistics ===" << std::endl;
    std::cout << "Total nodes:  " << nodes.size() << std::endl;
    std::cout << "Total leaves: " << leaves.size() << std::endl;
    std::cout << "Tree depth:   " << treeDepth << std::endl;
    std::cout << std::endl;

    // Count nodes at each depth
    std::vector<int> nodesPerLevel(treeDepth + 1, 0);
    std::vector<int> leavesPerLevel(treeDepth + 1, 0);

    countNodesAtDepth(0, 0, nodesPerLevel, leavesPerLevel);

    std::cout << "Nodes per level:" << std::endl;
    for (int i = 0; i <= treeDepth; i++) {
        float voxelSize = getVoxelSizeAtDepth(i);
        std::cout << "  Level " << i << " (voxel size " << std::setw(8) << voxelSize << "m): "
            << std::setw(6) << nodesPerLevel[i] << " nodes, "
            << std::setw(6) << leavesPerLevel[i] << " leaves" << std::endl;
    }
    std::cout << std::endl;
}

void TreeManager::countNodesAtDepth(uint32_t nodeIndex, int depth,
    std::vector<int>& nodesPerLevel,
    std::vector<int>& leavesPerLevel) {
    if (nodeIndex >= nodes.size()) return;

    nodesPerLevel[depth]++;

    uint32_t childPointer = nodes[nodeIndex].childPointer;

    if (childPointer & LEAF_NODE_FLAG) {
        // This is a leaf
        leavesPerLevel[depth]++;
    }
    else if (childPointer != 0) {
        // Has children - recurse into all 64 children
        for (uint32_t i = 0; i < 64; i++) {
            countNodesAtDepth(childPointer + i, depth + 1, nodesPerLevel, leavesPerLevel);
        }
    }
}

void TreeManager::printTree(uint32_t nodeIndex, int depth, std::string prefix, bool isLast) {
    if (nodeIndex >= nodes.size() || depth > 3) {  // Limit depth for readability
        return;
    }

    std::cout << prefix;
    std::cout << (isLast ? "└── " : "├── ");

    uint32_t childPointer = nodes[nodeIndex].childPointer;

    if (childPointer & LEAF_NODE_FLAG) {
        // Leaf node
        uint32_t leafIndex = childPointer & ~LEAF_NODE_FLAG;
        if (leafIndex < leaves.size()) {
            float dist = leaves[leafIndex].distance;
            std::cout << "LEAF [dist=" << std::fixed << std::setprecision(2) << dist << "]";
        }
        else {
            std::cout << "LEAF [invalid index]";
        }
    }
    else if (childPointer == 0) {
        std::cout << "EMPTY";
    }
    else {
        // Internal node with children
        std::cout << "NODE [" << 64 << " children at index " << childPointer << "]";

        // Only show first few children at shallow depths
        if (depth < 2) {
            for (uint32_t i = 0; i < 64 && i < 8; i++) {  // Show first 8
                std::string newPrefix = prefix + (isLast ? "    " : "│   ");
                bool childIsLast = (i == 7);
                printTree(childPointer + i, depth + 1, newPrefix, childIsLast);
            }
            if (depth == 0) {
                std::cout << prefix << (isLast ? "    " : "│   ") << "... (56 more children)" << std::endl;
            }
        }
    }

    std::cout << std::endl;
}

void TreeManager::visualizeTreeSlice(int sliceZ) {
    std::cout << "=== Tree Slice at Z=" << sliceZ << " ===" << std::endl;
    std::cout << "Legend: . = empty, # = solid, ~ = near surface" << std::endl;
    std::cout << std::endl;

    int gridSize = 32;  // Sample a 32x32 grid
    float range = 20.0f;  // Cover -10 to +10 in world space

    // Print Y axis (vertical)
    for (int y = gridSize - 1; y >= 0; y--) {
        float worldY = -range / 2 + (y * range / gridSize);
        std::cout << std::setw(5) << std::fixed << std::setprecision(1) << worldY << " ";

        for (int x = 0; x < gridSize; x++) {
            float worldX = -range / 2 + (x * range / gridSize);
            vec3 pos = { worldX, 0, worldY };  // Note: using Y as Z for 2D slice

            float dist = sampleDistanceAt(pos);

            char c;
            if (dist < -0.5f) {
                c = '#';  // Inside solid
            }
            else if (dist > 0.5f) {
                c = '.';  // Outside/empty
            }
            else {
                c = '~';  // Near surface
            }

            std::cout << c;
        }
        std::cout << std::endl;
    }

    // Print X axis
    std::cout << "      ";
    for (int x = 0; x < gridSize; x += 4) {
        float worldX = -range / 2 + (x * range / gridSize);
        std::cout << std::setw(4) << std::fixed << std::setprecision(0) << worldX;
    }
    std::cout << std::endl;
}

void TreeManager::printLeafDistribution() {
    std::cout << "=== Leaf Distance Distribution ===" << std::endl;

    std::map<int, int> histogram;

    for (const auto& leaf : leaves) {
        int bucket = (int)(leaf.distance / 2.0f);  // 2m buckets
        histogram[bucket]++;
    }

    for (const auto& [bucket, count] : histogram) {
        float minDist = bucket * 2.0f;
        float maxDist = (bucket + 1) * 2.0f;

        std::cout << std::setw(6) << std::fixed << std::setprecision(1) << minDist
            << " to " << std::setw(6) << maxDist << "m: ";

        // Simple bar chart
        int barLength = count / 10;  // Scale down
        for (int i = 0; i < barLength && i < 50; i++) {
            std::cout << "█";
        }
        std::cout << " " << count << std::endl;
    }
    std::cout << std::endl;
}
