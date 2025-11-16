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

        // TODO: use epsilon instead of 0
        if (result.depth >= treeDepth - 1 && result.voxel.distance > 0) {
            float distance = result.voxel.distance;

            // Calculate distance to exit voxel boundary
            float halfSize = result.voxelSize * 0.5;
            float3 voxelMin = result.voxelCenter - halfSize;
            float3 voxelMax = result.voxelCenter + halfSize;

            float3 t1 = (voxelMin - currentPosition) / currentDirection;
            float3 t2 = (voxelMax - currentPosition) / currentDirection;
            float3 tFar = max(t1, t2);
            float tExit = min(tFar.x, min(tFar.y, tFar.z));

            // Use whichever is bigger: distance to next voxel, or the SDF distance
            result.voxel.distance = max(tExit + epsilon, distance);
        }

        stepSize = result.voxel.distance;
        stepSize = smoothUnion(sceneSDF(currentPosition + float3(3, 0.8, 0)), result.voxel.distance, 0.4) - epsilon;

        stepSize = subtract(sceneSDF(currentPosition + float3(-3, 0, 0)), stepSize);

        //stepSize = sceneSDF(currentPosition);

        if (stepSize < 0.001) {
            hits++;
            break;

            // TODO: continue to make this functional
            // bounce the ray
            //float3 surfaceNormal = getNormal(currentPosition);
            //lastDirection = reflect(lastDirection, surfaceNormal);
            //break;
        }

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
    return sphere(pos - float3(0,-2,7), 1);
    //return repeatingSphere(pos - float3(3.5,3.5,0), 1, float3(7, 7, 7));
}
