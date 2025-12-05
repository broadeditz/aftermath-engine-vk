module raymarch;

import sdf;
import tree;

public struct raymarchResult {
  public float distance;
  public float lastStepSize;
  public int steps;
  public uint8_t hits;
  // hitPosition is the position of the point where the ray hit the surface
  public float3 hitPosition;
  // hitDirection is the direction the ray was going when it hit the surface
  public float3 hitDirection;
  // hitNormal is the normal of the surface hit by the ray,
  // not included for light probes
  public float3 hitNormal;
}

public raymarchResult raymarch(float3 rayOrigin, float3 rayDirection,
                               int maxSteps, float maxDistance, float epsilon,
                               StructuredBuffer<TreeNode> treeNodes,
                               StructuredBuffer<TreeLeaf> treeLeaves,
                               bool lightProbe) {
  float totalDistance = 0.0;
  float stepSize = 0.0;
  float3 currentPosition = rayOrigin;
  float3 currentDirection = rayDirection;
  int steps = 0;
  uint8_t hits = 0;

  // Precompute inverse direction once for efficiency
  float3 invDir = 1.0 / (currentDirection + float3(1e-5));

  float frequency = 0.00;
  float amplitude = 0.0;

  bool enableWobble = (frequency != 0 && amplitude != 0);

  float3 hitNormal = 0;

  for (steps = 0; steps < maxSteps; steps++) {
    if (totalDistance >= maxDistance) {
      break;
    }

    if (enableWobble) {
      // create perpendicular vectors to ray direction
      float3 right = normalize(cross(currentDirection, float3(0, 1, 0)));
      float3 up = normalize(cross(currentDirection, right));

      float3 offset = right * (sin(totalDistance * frequency) * amplitude) +
                      up * (cos(totalDistance * frequency) * amplitude);
      currentDirection = normalize(rayDirection + offset);
    };

    TreeSDFResult result = treeSDF(currentPosition, treeNodes, treeLeaves);

    // For positive distances, try to skip to voxel boundary
    if (result.voxel.distance > epsilon) {
      float halfSize = result.voxelSize * 0.5;
      float3 voxelMin = result.voxelCenter - halfSize;
      float3 voxelMax = result.voxelCenter + halfSize;

      // Proper ray-box intersection
      float3 clampedPos =
          clamp(currentPosition, voxelMin + epsilon, voxelMax - epsilon);
      float3 t1 = (voxelMin - clampedPos) * invDir;
      float3 t2 = (voxelMax - clampedPos) * invDir;

      float3 tNear = min(t1, t2);
      float3 tFar = max(t1, t2);

      float tExit = min(tFar.x, min(tFar.y, tFar.z));

      // Only use voxel exit if it's ahead of us and larger than SDF
      // Add small bias to ensure we actually exit the voxel
      if (tExit > 0) {
        float distanceFromCenter = length(currentPosition - result.voxelCenter);
        result.voxel.distance = max(result.voxel.distance - distanceFromCenter,
                                    tExit + epsilon * 2);
      }
    }

    stepSize = result.voxel.distance;
    // stepSize = smoothUnion(sceneSDF(currentPosition + float3(0, 0, 0)),
    // result.voxel.distance, 0.4) - epsilon;

    // stepSize = subtract(sceneSDF(currentPosition + float3(-3, 0, 0)),
    // stepSize);

    if (stepSize < epsilon) {
      hits++;
      if (lightProbe) {
        break;
      }

      // Use the ray-box intersection to determine which face was hit
      float halfSize = result.voxelSize * 0.5;
      float3 voxelMin = result.voxelCenter - halfSize;
      float3 voxelMax = result.voxelCenter + halfSize;

      float3 invDir = 1.0 / (currentDirection + float3(1e-8));
      float3 t1 = (voxelMin - currentPosition) * invDir;
      float3 t2 = (voxelMax - currentPosition) * invDir;

      float3 tNear = min(t1, t2);

      // Find which axis we hit (whichever has the maximum tNear)
      float tEntry = max(tNear.x, max(tNear.y, tNear.z));

      // Determine normal based on which face was hit
      if (tEntry == tNear.x) {
        hitNormal = float3(invDir.x > 0 ? -1 : 1, 0, 0);
      } else if (tEntry == tNear.y) {
        hitNormal = float3(0, invDir.y > 0 ? -1 : 1, 0);
      } else {
        hitNormal = float3(0, 0, invDir.z > 0 ? -1 : 1);
      }

      break;
    }

    currentPosition += currentDirection * stepSize;

    totalDistance += stepSize;
  }

  raymarchResult res;
  res.distance = totalDistance;
  res.lastStepSize = stepSize;
  res.steps = steps;
  res.hits = hits;
  res.hitPosition = currentPosition;
  res.hitDirection = currentDirection;
  res.hitNormal = hitNormal;
  return res;
}

float sceneSDF(float3 pos) {
  // return sphere(pos - float3(0, -2, 4.5), 1);
  return repeatingSphere(pos - float3(3.5, 3.5, 0), 1, float3(7, 7, 7));
}
