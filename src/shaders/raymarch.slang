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
    float3 lastPosition = rayOrigin;
    float3 lastDirection = rayDirection;
    int steps = 0;
    uint8_t hits = 0;

    float frequency = 0.00;
    float amplitude = 0.0;

    bool enableWobble = (frequency != 0 && amplitude != 0);

    for (steps = 0; steps < maxSteps; steps++) {
        if (enableWobble) {
            // create perpendicular vectors to ray direction
            float3 right = normalize(cross(lastDirection, float3(0, 1, 0)));
            float3 up = normalize(cross(lastDirection, right));

            //float3 offset = up * (cos(totalDistance * frequency) * amplitude);

            float3 offset = right * (sin(totalDistance * frequency) * amplitude) +
                            up * (cos(totalDistance * frequency) * amplitude);
            lastDirection = normalize(rayDirection + offset);
        };

        float3 currentPosition = lastPosition + lastDirection * stepSize;

        TreeSDFResult result = treeSDF(currentPosition, treeNodes, treeLeaves);

        //stepSize = result.voxel.distance;
        stepSize = smoothUnion(sceneSDF(currentPosition), result.voxel.distance, 0.4) - epsilon;

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

        totalDistance += stepSize;

        if (totalDistance >= maxDistance) {
            break;
        }
        lastPosition = currentPosition;
    }

    raymarchResult res;
    res.distance = totalDistance;
    res.lastStepSize = stepSize;
    res.steps = steps;
    res.hits = hits;
    return res;
}

float sceneSDF(float3 pos) {
    return sphere(pos - float3(3.5,3.5,7), 1);
    //return repeatingSphere(pos - float3(3.5,3.5,0), 1, float3(7, 7, 7));
}
