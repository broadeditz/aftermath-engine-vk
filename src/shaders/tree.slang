module tree;

public struct TreeNode {
    // 32-bit index to first child in array, if LEAF_FLAG is set, 
    // it means this is a leaf node, and contains the index for the leaf data instead.
    uint childPointer;
}

public struct TreeLeaf {
    float distance;        // Distance to nearest surface at this leaf
    MaterialType material; // Material type at this leaf, if distance <= 0
    uint8_t damage;        // Damage level at this leaf (0-255)
}

static const uint LEAF_FLAG = 0x80000000;

static const uint treeDepth = 3; // Max depth of the tree, before arriving at voxel resolution, directly determines max render distance.

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

    // TODO: implement loop

    if ((currentNode.childPointer & LEAF_FLAG) != 0) {
        return treeLeaves[currentNode.childPointer - LEAF_FLAG];
    }

    return treeLeaves[currentNode.childPointer];
}
