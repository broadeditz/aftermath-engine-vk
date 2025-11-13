#include <cstdint>

enum class MaterialType : uint8_t {
    Void = 0,
    Air = 1,
    Water = 2,
    Dirt = 3,
    Stone = 4,
    Grass = 5,
    Sand = 6,
    Wood = 7,
    Leaf = 8,
    Glass = 9,
    Torch = 10,
};

// Tree node structure - must match shader definition
struct TreeNode {
    uint64_t childMask;
    uint32_t childPointer;
	uint32_t padding; // Padding for alignment, not needed, but good to be reminded that we can use another 32-bit value here if needed
};

struct TreeLeaf {
    float distance;
    MaterialType material;
};

const uint32_t LEAF_NODE_FLAG = 0x80000000;
