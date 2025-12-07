#include "tree.hpp"


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

    if (nodes[nodeIndex].flags & LEAF_NODE_FLAG) {
        if (nodes[nodeIndex].flags & LOD_NODE_FLAG) {
	        // This is a regular leaf
	        leavesPerLevel[depth + 1] += 64;
        } else {
        	// This is a sparsity leaf
	        leavesPerLevel[depth]++;
        }
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

    if (nodes[nodeIndex].flags & LEAF_NODE_FLAG) {
        // Leaf node
        uint32_t leafIndex = childPointer;
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
