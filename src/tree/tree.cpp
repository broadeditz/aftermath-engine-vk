#include "tree.hpp"

const int treeDepth = 3; // Example depth for test tree
const float baseVoxelSize = 1.0f; // Size of the smallest voxel at the deepest level

float getVoxelSizeAtDepth(int depth) {
	return baseVoxelSize * pow(4, treeDepth - depth);
}

// Function to compute chunk center position based on sexagintaquartant index within the known parent chunk.
// Chunk indexing is as follows:
// Index 0-3 row 0 of layer 0
// Index 4-7 row 1 of layer 0
// ...

// This function is definitely wrong
vec3 getChunkPosition(uint32_t chunkIndex, float voxelSize, vec3 parentPosition) {
	// Validate input
	if (chunkIndex >= 64) {
		// Return parent position for invalid index
		return parentPosition;
	}

	// Extract 3D coordinates from the linear index
	// The subdivision is 4󫶘, so we have:
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

float length(vec3 pos) {
	return sqrt(pos.x * pos.x + pos.y * pos.y + pos.z * pos.z);
}

vec3 sub(vec3 pos, vec3 move) {
	return { pos.x - move.x, pos.y - move.y, pos.z - move.z };
}

float sampleDistanceAt(vec3 position) {
	// Simple test example: distance increases with height, flat floor at -2 y
	return position.y + 2;

	// simple sphere
	//return length(sub(position, vec3{0, 0, -5})) - 5;
}

uint32_t TreeManager::createLeaf(float distance) {
	MaterialType material;
	if (distance < 0.01) {
		material = MaterialType::Grass;
	}

	TreeLeaf leaf = {
		.distance = distance,
		.material = material,
	};

	uint32_t leafPointer = leaves.size();
	leaves.push_back(leaf);

	return leafPointer;
}

// Create children for a node and add them to the processing queue
void TreeManager::subdivideNode(
	uint32_t parentIndex,
	int depth,
	vec3 parentPosition
) {
	float distance = sampleDistanceAt(parentPosition);
	float voxelSize = getVoxelSizeAtDepth(depth-1);

	// create sparsity leaf if the nearest surface is further than the size of the node
	if (abs(distance) > voxelSize) {
		uint32_t leafPointer = createLeaf(distance);

		nodes[parentIndex].childPointer = leafPointer | LEAF_NODE_FLAG;

		return;
	}

	// create voxel leaf if at smallest possible voxel resolution
	if (depth == treeDepth) {
		uint32_t leafPointer = createLeaf(distance);

		nodes[parentIndex].childPointer = leafPointer | LEAF_NODE_FLAG;

		return;
	}

	// set pointer of parent to the first child -- which hasn't been created yet at this point(!)
	if (nodes[parentIndex].childPointer == 0) {
		uint32_t childPointer = nodes.size();
		nodes[parentIndex].childPointer = childPointer;
	}

	voxelSize = getVoxelSizeAtDepth(depth);

	// Create 64 children (4󫶘 subdivision)
	for (uint32_t i = 0; i < 64; i++) {
		vec3 childPosition = getChunkPosition(i, voxelSize, parentPosition);

		TreeNode childNode = {};
		childNode.childPointer = 0;

		uint32_t childIndex = nodes.size();
		nodes.push_back(childNode);

		// Add child to queue for further processing
		queue.push_back(nodeToProcess{ childIndex, depth + 1, childPosition });
	}
}

void TreeManager::createTestTree() {
	nodes.clear();
	leaves.clear();
	// Simple test tree with one root and 8 leaf children
	TreeNode rootNode = {};
	rootNode.childPointer = 1; // First child at index 1
	nodes.push_back(rootNode);

	uint32_t leafIndex = 0;
	float voxelSize = getVoxelSizeAtDepth(0);

	vec3 rootPosition = {
		.x = 0.0,
		.y = 0.0,
		.z = 0.0,
	};

	// Create 64 children (4󫶘 subdivision)
	for (uint32_t i = 0; i < 64; i++) {
		vec3 childPosition = getChunkPosition(i, voxelSize, rootPosition);

		TreeNode childNode = {};
		childNode.childPointer = 0;

		nodes.push_back(childNode);

		// Add child to queue for further processing
		queue.push_back(nodeToProcess{ 0, 1, childPosition });
	}

	while (!queue.empty()) {
		nodeToProcess current = queue.front();
		queue.erase(queue.begin());

		subdivideNode(current.parentNodeIndex, current.depth, current.parentPosition);
	}
}
