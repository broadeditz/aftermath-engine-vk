import raymarch;
import sdf;
import tree;

struct VertexInput {
  float2 inPosition;
  float2 inTexCoord;
};

struct VertexOutput {
  float4 pos : SV_Position;
  float2 texCoord;
};

[shader("vertex")]
VertexOutput vertMain(VertexInput input) {
  VertexOutput output;
  output.pos = float4(input.inPosition, 0.0, 1.0);
  output.texCoord = input.inTexCoord;
  return output;
}

// Binding 0 set 0
[[vk::binding(0, 0)]]
Texture2D ComputeOutput;
// Binding 1 set 0
[[vk::binding(1, 0)]]
SamplerState ComputeOutputSampler;

[shader("fragment")]
float4 fragMain(VertexOutput vertIn) : SV_Target {
  return ComputeOutput.Sample(ComputeOutputSampler, vertIn.texCoord);
}

struct FrameUniforms {
  float time;
  // aperture is the diameter of the camera aperture, in world units
  float aperture;
  // focusDistance is the distance from the camera to the focal plane, in world
  // units
  float focusDistance;
  // fov is the magnification factor for the camera, larger values mean a
  // narrower field of view
  float fov;

  float3 cameraPosition;
  float3 cameraDirection;
};

struct RenderUniforms {
  int maxSteps;
  int maxBounces;
  float maxDistance;
  // epsilon is the minimum distance to consider a hit
  float epsilon;
  // ray samples per pixel
  int samples;
}

// Binding 3 set 0
[[vk::binding(3, 0)]]
StructuredBuffer<TreeNode> treeNodes;

// Binding 4 set 0
[[vk::binding(4, 0)]]
StructuredBuffer<TreeLeaf> treeLeaves;

// Binding 2 set 0
[[vk::binding(2, 0)]]
RWTexture2D<float4> outputImage;

// Binding 0 set 1
[[vk::binding(0, 1)]]
ConstantBuffer<FrameUniforms> frameUniforms;

// Simple hash function for pseudo-random numbers
float hash(float2 p) {
  float3 p3 = frac(float3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return frac((p3.x + p3.y) * p3.z);
}

// Vogel disk sampling - evenly distributed points in a disk
float2 vogelDiskSample(int sampleIndex, int numSamples, float rotation) {
  const float goldenAngle = 2.39996323; // 2*PI / golden ratio
  float r = sqrt(float(sampleIndex) + 0.5) / sqrt(float(numSamples));
  float theta = float(sampleIndex) * goldenAngle;

  // Rotate the pattern per pixel to break up repetition
  theta += rotation;

  // TODO: investigate this sneaking suspicion that r messes with the rotation
  return float2(cos(theta), sin(theta)) * r;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {
  uint2 pixelCoords = dispatchThreadID.xy;

  uint width, height;
  outputImage.GetDimensions(width, height);

  if (pixelCoords.x >= width || pixelCoords.y >= height) {
    return;
  }

  float aspectRatio = float(width) / float(height);

  int samplesPerPixel = 1;

  // Calculate position on the image plane (focal plane)
  float2 uv =
      float2((float(pixelCoords.x) / float(width) - 0.5) * 2.0 * aspectRatio,
             (float(pixelCoords.y) / float(height) - 0.5) * 2.0);

  float3 cameraPosition = frameUniforms.cameraPosition;

  float3 forward = normalize(frameUniforms.cameraDirection);

  // Calculate right and up vectors for the camera
  float3 worldUp = float3(0, 1, 0);
  float3 right = normalize(cross(forward, worldUp));
  float3 up = normalize(cross(right, forward));

  // Calculate the center direction (no focal point yet, just the view
  // direction)
  float3 centerDirection =
      normalize(forward * frameUniforms.fov + right * uv.x + up * uv.y);

  // Focal point for depth of field
  float3 focalPoint =
      cameraPosition + centerDirection * frameUniforms.focusDistance;

  float vogelOffset = hash(float2(pixelCoords) + frameUniforms.time * 100);

  float3 fogAccum = float3(0, 0, 0);
  float3 colorAccum = float3(0.0, 0.0, 0.0);
  for (int i = 0; i < samplesPerPixel; i++) {
    // Apply aperture offset in camera space (perpendicular to view direction)
    float2 apertureOffset = vogelDiskSample(i, samplesPerPixel, vogelOffset) *
                            frameUniforms.aperture;
    float3 rayOrigin =
        cameraPosition + right * apertureOffset.x + up * apertureOffset.y;

    // Final ray direction: from offset origin TO the fixed focal point
    float3 rayDirection = normalize(focalPoint - rayOrigin);

    // note: negative epsilon ray march values could theoretically be used to
    // see through voxels up to a given depth, probably doesn't work right now.
    raymarchResult result = raymarch(rayOrigin, rayDirection, 200, 100000.0,
                                     0.0001, treeNodes, treeLeaves, false);

    if (result.hits > 0) {
      float3 sunDirection = normalize(float3(0.4, 1, 0.3));
      float3 skyColor = float3(0.5, 0.7, 1.0);
      float3 sunColor = float3(1.0, 0.95, 0.9);

      float3 lighting = float3(0);

      // Diffuse lighting from sun
      float normalDotSun = max(0.0, dot(result.hitNormal, sunDirection));

      if (normalDotSun > 0.001) {
        float3 shadowOrigin = result.hitPosition + result.hitNormal * 0.02;
        raymarchResult shadowProbe =
            raymarch(shadowOrigin, sunDirection, 50, 1000, 0.0001, treeNodes,
                     treeLeaves, true);

        // Soft shadows - clamp to [0, 1]
        float shadowFactor =
            shadowProbe.hits > 0
                ? 0.0
                : clamp(1.0 - (float(shadowProbe.steps) / 50.0) * 0.3, 0.0,
                        1.0);

        lighting += sunColor * normalDotSun * shadowFactor *
                    0.8; // Scale down sun intensity
      }

      // Ambient occlusion - ensure steps > 0
      float stepCount = max(1.0, float(result.steps));
      float ao = 1.0 / (1.0 + pow(stepCount * 0.1, 0.7));
      ao = clamp(ao, 0.0, 1.0);

      // Sky/ambient lighting
      float skyAmount = clamp(result.hitNormal.y * 0.5 + 0.5, 0.0, 1.0);
      lighting += skyColor * 0.3 * ao * skyAmount;

      // Ground bounce
      float groundAmount = clamp(-result.hitNormal.y * 0.5 + 0.5, 0.0, 1.0);
      lighting += float3(0.4, 0.3, 0.2) * 0.15 * ao * groundAmount;

      // Distance fog with safe exponential
      // float fogAmount = clamp(1.0 - exp(-result.distance * 0.001), 0.0, 1.0);
      // lighting = lerp(lighting, skyColor * 0.8, fogAmount);

      // Clamp final lighting to reasonable range
      lighting = clamp(lighting, 0.0, 1.0);

      colorAccum += lighting;
    }

    fogAccum += min(result.distance / 5000, 1);
  }

  outputImage[pixelCoords] = float4(colorAccum / float(samplesPerPixel) +
                                        fogAccum * 0.5 / float(samplesPerPixel),
                                    1.0);
}
