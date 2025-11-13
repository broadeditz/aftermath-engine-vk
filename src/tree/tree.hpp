#include <cstdint>
#include <vector>

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
    uint32_t childPointer; // Pointer to first child, if LEAF_NODE_FLAG is set, it means it's a leaf node, and contains the index for the leaf data.
};

struct TreeLeaf {
	float distance; // Distance from block center to nearest surface, can be negative
    MaterialType material;
	uint8_t damage; // damage to the block, 0-255
	uint8_t padding[2]; // Padding for alignment, not needed, but good to be reminded that we can use another 16 bits worth of data here if needed
};

const uint32_t LEAF_NODE_FLAG = 0x80000000;

struct vec3 {
    float x, y, z;
};

struct nodeToProcess {
    uint32_t parentNodeIndex;
    int depth;
    vec3 parentPosition;
};

class TreeManager {
public:
	std::vector<TreeNode> nodes;
	std::vector<TreeLeaf> leaves;

private:
    std::vector<nodeToProcess> queue;

    void createTestTree();
    uint32_t createLeaf(float distance);
    void subdivideNode(uint32_t parentIndex, int parentDepth, vec3 parentPosition);
	// TODO: store freed indices for reuse
};