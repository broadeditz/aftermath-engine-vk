module tree;

public struct TreeNode {
    uint64_t childMask;  // Which of 64 children exist (1 bit each)
    uint childPointer;  // 32-bit index to first child in array
}

public struct TreeLeaf {
    float distance;        // Distance to nearest surface at this leaf
    MaterialType material; // Material type at this leaf, if distance <= 0
}

static const uint LEAF_FLAG = 0x80000000;

enum MaterialType :uint8_t {
    Void = 0,
    Air = 1, // unused for now, void is air
    Water = 2,
    Dirt = 3,
    Stone = 4,
    Grass = 5,
    Sand = 6,
    Wood = 7,
    Leaf = 8,
    Glass = 9
}

public struct TreeSDFResult {
    TreeNode voxel;
    float distance;
}

StructuredBuffer<TreeNode> treeNodes;
StructuredBuffer<TreeLeaf> treeLeaves;

public TreeLeaf treeSDF(float3 worldPos) {
    TreeNode currentNode = treeNodes[0];

    if ((currentNode.childMask & LEAF_FLAG) != 0) {
        return treeLeaves[currentNode.childPointer];
    }

    return treeLeaves[currentNode.childPointer];
}
