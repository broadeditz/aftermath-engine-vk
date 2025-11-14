module tree;

public struct TreeNode {
    // 32-bit index to first child in the buffer, all 64 children are guaranteed to be contiguously indexed.
    // If LEAF_FLAG is set, it means this is a leaf node, and contains the index for the leaf data instead.
    // If it is a leaf node, it only has 1 leaf data node, not 64.
    uint childPointer;
}

public struct TreeLeaf {
    public float distance;        // Distance to nearest surface at this leaf
    public MaterialType material; // Material type at this leaf, if distance <= 0
    public uint8_t damage;        // Damage level at this leaf (0-255)
}

static const uint LEAF_FLAG = 0x80000000;

static const uint treeDepth = 5; // Max depth of the tree, before arriving at voxel resolution, directly determines max render distance.
static const float voxelSize = 0.25; // resolution of smallest voxel at the deepest point of the tree.

public enum MaterialType : uint8_t {
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

static const float3 rootOrigin = { 0, 0, 0 };

public TreeLeaf treeSDF(float3 worldPos, StructuredBuffer<TreeNode> treeNodes, StructuredBuffer<TreeLeaf> treeLeaves) {
    TreeNode currentNode = treeNodes[0];

    float3 nodeCenter = rootOrigin;

    float nodeSize = pow(4, float(treeDepth)) * voxelSize;

    for (uint depth = 0; depth <= treeDepth; depth++) {
        if (bool(currentNode.childPointer & LEAF_FLAG)) {
            uint leafIndex = currentNode.childPointer & ~LEAF_FLAG;
            TreeLeaf leaf = treeLeaves[leafIndex];

            // adjust distance based on offset from center of the node
            //float3 offset = abs(worldPos - nodeCenter);
            //leaf.distance = leaf.distance - length(offset);

            // Apply conservative bound while preserving sign
            float halfDiagonal = nodeSize * sqrt(3.0) * 0.5;
            float sign = leaf.distance >= 0 ? 1.0 : -1.0;
            float conservativeMagnitude = abs(leaf.distance) - halfDiagonal;

            // Clamp magnitude to ensure forward progress, keep sign
            leaf.distance = sign * max(conservativeMagnitude, voxelSize * 0.1);

            return leaf;
        }

        float3 relativePos = worldPos - nodeCenter;


        nodeSize *= 0.25; // 4x4x4 tree matrix means 1/4th the voxel edge size for every depth step
        // map position to 4x4x4 matrix, so we know which child to traverse to
        uint xIndex = clamp(uint(floor((relativePos.x / nodeSize) + 2.0)), 0, 3);
        uint yIndex = clamp(uint(floor((relativePos.y / nodeSize) + 2.0)), 0, 3);
        uint zIndex = clamp(uint(floor((relativePos.z / nodeSize) + 2.0)), 0, 3);

        uint childIndex = zIndex * 16 + yIndex * 4 + xIndex;


        // calculate child node's center point
        nodeCenter.x += (xIndex - 1.5) * nodeSize;
        nodeCenter.y += (yIndex - 1.5) * nodeSize;
        nodeCenter.z += (zIndex - 1.5) * nodeSize;

        uint childPointer = currentNode.childPointer + childIndex;
        currentNode = treeNodes[childPointer];
    }

    // Fallback: return default leaf
    TreeLeaf defaultLeaf;
    defaultLeaf.distance = 10000000.0; // huge distance so max ray distance instantly gets triggered & ray marching stops.
    defaultLeaf.material = MaterialType::Void;
    defaultLeaf.damage = 0;
    return defaultLeaf;
}
