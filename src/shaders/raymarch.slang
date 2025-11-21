module raymarch;

import sdf;
import tree;

public struct raymarchResult {
    public float distance;
    public float lastStepSize;
    public int steps;
    public uint8_t hits;
}

float3 getNormal(float3 pos) {
    // Tetrahedral normal estimation
    float2 k = float2(1, -1);
    float h = 0.001;
    return normalize(
        k.xyy * sceneSDF(pos + k.xyy * h) +
        k.yyx * sceneSDF(pos + k.yyx * h) +
        k.yxy * sceneSDF(pos + k.yxy * h) +
        k.xxx * sceneSDF(pos + k.xxx * h)
    );
}

public raymarchResult raymarch(float3 rayOrigin, float3 rayDirection, int maxSteps, float maxDistance, float epsilon, StructuredBuffer<TreeNode> treeNodes, StructuredBuffer<TreeLeaf> treeLeaves) {
    float totalDistance = 0.0;
    float stepSize = 0.0;
    float3 currentPosition = rayOrigin;
    float3 currentDirection = rayDirection;
    int steps = 0;
    uint8_t hits = 0;

    // Precompute inverse direction once for efficiency
    float3 invDir = 1.0 / (currentDirection + float3(1e-8));

    float frequency = 0.00;
    float amplitude = 0.0;

    bool enableWobble = (frequency != 0 && amplitude != 0);

    for (steps = 0; steps < maxSteps; steps++) {
        if (totalDistance >= maxDistance) {
            break;
        }

        if (enableWobble) {
            // create perpendicular vectors to ray direction
            float3 right = normalize(cross(currentDirection, float3(0, 1, 0)));
            float3 up = normalize(cross(currentDirection, right));

            //float3 offset = up * (cos(totalDistance * frequency) * amplitude);

            float3 offset = right * (sin(totalDistance * frequency) * amplitude) +
                            up * (cos(totalDistance * frequency) * amplitude);
            currentDirection = normalize(rayDirection + offset);
        };

        TreeSDFResult result = treeSDF(currentPosition, treeNodes, treeLeaves);

        // Calculate distance to exit voxel boundary

        // For positive distances, try to skip to voxel boundary
        if (result.voxel.distance > epsilon) {
            float halfSize = result.voxelSize * 0.5;
            //float3 voxelMin = result.voxelCenter;
            float3 voxelMin = result.voxelCenter - halfSize;
            //float3 voxelMax = result.voxelCenter + 2*halfSize;
            float3 voxelMax = result.voxelCenter + halfSize;

            // Proper ray-box intersection
            //float3 t1 = (voxelMin - currentPosition + float3(0.0, 0.5 *voxelSize, 0.0)) * invDir;
            //float3 t2 = (voxelMax - currentPosition + float3(0.0, 0.5*voxelSize, 0.0)) * invDir;
            float3 t1 = (voxelMin - currentPosition) * invDir;
            float3 t2 = (voxelMax - currentPosition) * invDir;

            float3 tNear = min(t1, t2);
            float3 tFar = max(t1, t2);

            float tExit = min(tFar.x, min(tFar.y, tFar.z));

            // Only use voxel exit if it's ahead of us and larger than SDF
            // Add small bias to ensure we actually exit the voxel
            if (tExit > epsilon * 0.1) {
                if (bool(result.voxel.flags & LOD_FLAG)) {
                    result.voxel.distance = tExit + epsilon;
                    //result.voxel.distance = min(abs(result.voxel.distance), tExit + epsilon);
                } else {
                    result.voxel.distance = max(result.voxel.distance, tExit + epsilon);
                }
            }
        }
        //} else if (result.voxel.distance < 0) {
            // Negative distance - marching backward to find surface
            //float3 tNear = min(t1, t2);
            //float tEntry = max(tNear.x, max(tNear.y, tNear.z));
            //result.voxel.distance = max(tEntry - epsilon, distance);
        //}

        stepSize = result.voxel.distance;
        //stepSize = smoothUnion(sceneSDF(currentPosition + float3(0, 0, 0)), result.voxel.distance, 0.4) - epsilon;

        //stepSize = subtract(sceneSDF(currentPosition + float3(-3, 0, 0)), stepSize);

        //stepSize = sceneSDF(currentPosition);

        if (stepSize < epsilon) {
            hits++;
            break;

            // TODO: continue to make this functional
            // bounce the ray
            //float3 surfaceNormal = getNormal(currentPosition);
            //lastDirection = reflect(lastDirection, surfaceNormal);
            //break;
        }

        //stepSize = max(stepSize, epsilon * 5.0);

        currentPosition += currentDirection * stepSize;

        totalDistance += stepSize;
    }

    raymarchResult res;
    res.distance = totalDistance;
    res.lastStepSize = stepSize;
    res.steps = steps;
    res.hits = hits;
    return res;
}

float sceneSDF(float3 pos) {
    return sphere(pos - float3(0,-2,4.5), 1);
    //return repeatingSphere(pos - float3(3.5,3.5,0), 1, float3(7, 7, 7));
}
