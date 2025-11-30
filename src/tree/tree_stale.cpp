#include "tree.hpp"
#include "buffer.hpp"

#include <glm/glm.hpp>

void TreeManager::moveObserver(vec3 pos) {
    vec3 delta = sub(pos, observerPos);
    float movementDistance = length(delta);

    // Only update if movement is significant (more than 10 units)
    // Adjust this threshold based on your game's scale
    const float updateThreshold = 10.0f;

    if (movementDistance < updateThreshold) {
        return;
    }

    std::cout << "Observer moved " << movementDistance << " units, marking stale nodes..." << std::endl;

    observerPos = pos;
    lastObserverUpdateDistance = movementDistance;

    // Mark all LOD leaf nodes as potentially stale
    // We'll traverse the tree and check which nodes need LOD changes
    markStaleRecursive(0, 0, rootPosition);

    std::cout << "Marked stale nodes for update" << std::endl;
}

void TreeManager::markStaleRecursive(uint32_t nodeIndex, int depth, vec3 nodePosition) {
    if (nodeIndex >= nodes.size()) return;

    uint32_t childPointer = nodes[nodeIndex].childPointer;

    // Check if this is a LOD leaf node
    if (childPointer & LEAF_NODE_FLAG) {
        uint32_t leafIndex = childPointer & ~LEAF_NODE_FLAG;
        if (leafIndex < leaves.size()) {
            // Check if this leaf has the LOD flag set
            if (leaves[leafIndex].flags & 1) {
                float oldDistance = length(sub(nodePosition, sub(observerPos, sub(observerPos, sub(observerPos, {0,0,0}))))); // old position calculation
                float newDistance = length(sub(nodePosition, observerPos));

                int oldLOD = calculateLOD(treeDepth, oldDistance - lastObserverUpdateDistance, 128);
                int newLOD = calculateLOD(treeDepth, newDistance, 128);

                // If LOD changed, mark node as stale with its metadata
                if (oldLOD != newLOD) {
                    markStaleNode({nodeIndex, depth, nodePosition});
                }
            }
        }
        return;
    }

    // Recurse into children if it's an internal node
    if (childPointer != 0) {
        float voxelSize = getVoxelSizeAtDepth(depth + 1);
        for (uint32_t i = 0; i < 64; i++) {
            vec3 childPosition = getChunkPosition(i, voxelSize, nodePosition);
            markStaleRecursive(childPointer + i, depth + 1, childPosition);
        }
    }
}

void TreeManager::markStaleNode(nodeToProcess metadata) {
    staleQueue.send(metadata);
}

void TreeManager::updateStaleLODs() {
	if (staleQueue.size() == 0) {
       	return;
    }

    std::cout << "Processing stale nodes..." << std::endl;

    // Receive all stale nodes from the queue
    std::vector<nodeToProcess> nodesToReprocess;

    nodeToProcess metadata;
    // Drain the stale queue
    while (staleQueue.receive(metadata)) {
        nodesToReprocess.push_back(metadata);

        if (staleQueue.size() == 0) {
        	break;
        }
    }

    if (nodesToReprocess.empty()) {
        return;
    }

    std::cout << "Updating " << nodesToReprocess.size() << " stale nodes..." << std::endl;

    // Clear the stale nodes' children so they'll be rebuilt
    for (const auto& node : nodesToReprocess) {
    	// recursively free node & its children & leaves
        // TODO: ensure we only free nodes that are the first child of their parent
    	freeNode(node.parentNodeIndex, 0);
    }

    // Requeue nodes for reprocessing using existing worker infrastructure
    wg.add(nodesToReprocess.size());
    queue.sendMany(nodesToReprocess);

    // Workers are already running, they'll pick up the new work
    // Wait for reprocessing to complete
    wg.wait();

    std::cout << "LOD update complete - reprocessed " << nodesToReprocess.size() << " nodes" << std::endl;
}
