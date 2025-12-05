module tree;

public struct TreeNode {
  uint64_t childMask; // 64-bit mask of child nodes, each bit represents a child
                      // node, 0 means the child is coalesced with neighboring
                      // 0s, 1 means the child is a standalone leaf.
  // 32-bit index to first child in the buffer, all 64 children are guaranteed
  // to be contiguously indexed. If LEAF_FLAG is set, it means this is a leaf
  // node, and contains the index for the leaf data instead. If it is a leaf
  // node, it only has 1 leaf data node, not 64.
  uint childPointer;
  uint padding; // 32 bits padding to ensure alignment, can be used for future
                // purposes.
}

public struct TreeLeaf {
  public float distance;        // Distance to nearest surface at this leaf
  public MaterialType material; // Material type at this leaf, if distance <= 0
  public uint8_t damage;        // Damage level at this leaf (0-255)
  public uint8_t flags;         // Bitwise flags with metadata about the leaf
}

public static const uint LOD_FLAG = 0x01;

static const uint LEAF_FLAG = 0x80000000;

public static const uint treeDepth =
    9; // Max depth of the tree, before arriving at voxel resolution, directly
       // determines max render distance.

// Use integer to represent voxel size
// VOXEL_SIZE_NUM = numerator, VOXEL_SIZE_DENOM = denominator
// e.g., 0.25 = 1/4, so NUM=1, DENOM=4
public struct TreeConstants<let DEPTH : int, let VOXEL_SIZE_NUM : int,
                            let VOXEL_SIZE_DENOM : int> {
  static const float voxelSize =
      float(VOXEL_SIZE_NUM) / float(VOXEL_SIZE_DENOM);
  static const float rootNodeSize = voxelSize * pow(4.0, float(DEPTH));
  static const float sqrtThree = 1.732050808;
  static const float halfSqrtThree = 0.866025404;
  static const float voxelSafetyMargin = voxelSize * 0.1;

  public static float getNodeSize(uint depth) {
    return voxelSize * pow(4.0, float(DEPTH - depth));
  }

  public static float getInvNodeSize(uint depth) {
    return 1.0 / getNodeSize(depth);
  }

  public static float getHalfDiagonal(uint depth) {
    return getNodeSize(depth) * halfSqrtThree;
  }
}

public typealias treeConstants = TreeConstants<treeDepth, 1, 4>;

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
  public TreeLeaf voxel;
  public uint depth;
  public float voxelSize;
  public float3 voxelCenter;
}

static const float3 rootOrigin = { 0, 0, 0 };

public TreeSDFResult treeSDF(float3 worldPos,
                             StructuredBuffer<TreeNode> treeNodes,
                             StructuredBuffer<TreeLeaf> treeLeaves) {
  TreeNode currentNode = treeNodes[0];

  float3 nodeCenter = rootOrigin;

  float nodeSize = treeConstants::rootNodeSize;

  for (uint depth = 0; depth <= treeDepth; depth++) {
    if (bool(currentNode.childPointer & LEAF_FLAG)) {
      uint leafIndex = currentNode.childPointer & ~LEAF_FLAG;
      TreeLeaf leaf = treeLeaves[leafIndex];

      TreeSDFResult result;
      result.voxel = leaf;
      result.depth = depth;
      result.voxelSize = treeConstants.getNodeSize(depth);
      result.voxelCenter = nodeCenter;
      return result;
    }

    float3 relativePos = worldPos - nodeCenter;

    nodeSize = treeConstants::getNodeSize(
        depth + 1); // 4x4x4 tree matrix means 1/4th the voxel edge size for
                    // every depth step
    float invNodeSize = treeConstants::getInvNodeSize(depth + 1);

    // map position to 4x4x4 matrix, so we know which child to traverse to
    // using inverted node size, because multiplication is cheaper than division
    uint xIndex = clamp(uint(floor((relativePos.x * invNodeSize) + 2.0)), 0, 3);
    uint yIndex = clamp(uint(floor((relativePos.y * invNodeSize) + 2.0)), 0, 3);
    uint zIndex = clamp(uint(floor((relativePos.z * invNodeSize) + 2.0)), 0, 3);

    uint childIndex = (zIndex << 4) + (yIndex << 2) + xIndex;

    // precomputed voxel offsets
    static const float offsets[4] = { -1.5, -0.5, 0.5, 1.5 };

    // calculate child node's center point
    nodeCenter.x += offsets[xIndex] * nodeSize;
    nodeCenter.y += offsets[yIndex] * nodeSize;
    nodeCenter.z += offsets[zIndex] * nodeSize;

    uint childPointer = currentNode.childPointer + childIndex;
    currentNode = treeNodes[childPointer];
  }

  // Fallback: return default leaf
  TreeLeaf defaultLeaf;
  defaultLeaf.distance =
      10000000.0; // huge distance so max ray distance instantly gets triggered
                  // & ray marching stops.
  defaultLeaf.material = MaterialType::Void;
  defaultLeaf.damage = 0;

  TreeSDFResult result;
  result.voxel = defaultLeaf;
  result.depth = 0;
  return result;
}
