module octree;

public struct OctreeNode {
    uint childPointer;  // Index to first child in array
    uint parentPointer; // Index of parent node
    uint8_t childMask;  // Which of 8 children exist (1 bit each)

    uint8_t depth; // How deep in the tree are we? Eg. 0 = root, x = voxel

    MaterialType material;
};

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

public struct OctreeSDFResult {
    OctreeNode voxel;
    float distance;
}

StructuredBuffer<OctreeNode> octreeNodes;

public float octreeSDF(float3 worldPos) {
    return 0;
}

OctreeNode findNearestVoxel(float3 worldPos) {
    OctreeNode result;
    return result;
}