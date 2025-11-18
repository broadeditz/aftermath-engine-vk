import raymarch;
import sdf;
import tree;

struct VertexInput {
    float2 inPosition;
    float2 inTexCoord;
};

struct VertexOutput
{
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
    // focusDistance is the distance from the camera to the focal plane, in world units
    float focusDistance;
    // fov is the magnification factor for the camera, larger values mean a narrower field of view
    float fov;

    float3 cameraPosition;
    float2 cameraDirection;
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

float mapRange(float value)
{
    // Clamp input to ensure it's in [-1, 1]
    value = clamp(value, -1.0, 1.0);

    // Map from [-1, 1] to [-5, 0.001]
    // Formula: output = ((value - inputMin) / (inputMax - inputMin)) * (outputMax - outputMin) + outputMin
    float inputMin = -1.0;
    float inputMax = 1.0;
    float outputMin = -3.0;
    float outputMax = 0.001;

    float normalized = (value - inputMin) / (inputMax - inputMin); // Maps to [0, 1]
    float result = normalized * (outputMax - outputMin) + outputMin;

    return result;
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

    int samplesPerPixel = 3;

    // Calculate position on the image plane (focal plane)
    float2 uv = float2(
        (float(pixelCoords.x) / float(width) - 0.5) * 2.0 * aspectRatio,
        (float(pixelCoords.y) / float(height) - 0.5) * 2.0
    );

    float3 cameraPosition = frameUniforms.cameraPosition;
    //float3 cameraPosition = float3(0.0, 0.0, frameUniforms.time * 0.4);

    // Convert camera direction (yaw, pitch) to a forward vector
    float yaw = frameUniforms.cameraDirection.x;
    float pitch = frameUniforms.cameraDirection.y;

    float3 forward = float3(
        cos(yaw) * sin(pitch),
        sin(yaw),
        cos(yaw) * cos(pitch)
    );

    // Calculate right and up vectors for the camera
    float3 worldUp = float3(0, 1, 0);
    float3 right = normalize(cross(worldUp, forward));
    float3 up = cross(forward, right);

    // Point on the image plane using camera basis vectors
    float3 imagePoint = cameraPosition + 
                        forward * frameUniforms.fov + 
                        right * uv.x + 
                        up * uv.y;

    float3 centerDirection = normalize(imagePoint - cameraPosition);

    // Focal point at the desired focus distance (this is where all rays should converge)
    float3 focalPoint = cameraPosition + centerDirection * frameUniforms.focusDistance;

    float vogelOffset = hash(float2(pixelCoords) + frameUniforms.time * 100);

    float3 fogAccum = float3(0, 0, 0);
    float3 colorAccum = float3(0.0, 0.0, 0.0);
    for (int i = 0; i < samplesPerPixel; i++) {
        // apply aperture offset to ray origin
        float2 apertureOffset = vogelDiskSample(i, samplesPerPixel, vogelOffset) * frameUniforms.aperture;
        float3 rayOrigin = cameraPosition + float3(apertureOffset.x, apertureOffset.y, 0.0);

        // Final ray direction: from offset origin TO the fixed focal point
        float3 rayDirection = normalize(focalPoint - rayOrigin);

        // note: negative epsilon ray march values could theoretically be used to see through voxels up to a given depth
        //raymarchResult result = raymarch(rayOrigin, rayDirection, 500, 100000.0, -1, treeNodes, treeLeaves);
        //raymarchResult result = raymarch(rayOrigin, rayDirection, 200, 100000.0, mapRange(sin(frameUniforms.time * 0.1)), treeNodes, treeLeaves);
        raymarchResult result = raymarch(rayOrigin, rayDirection, 100, 100000.0, 0.005, treeNodes, treeLeaves);

        if (result.hits > 0) {
            float smoothSteps = float(result.steps);
            smoothSteps++; // prevent divide by 0
            smoothSteps = pow(smoothSteps, 0.7);
            // float distanceFade = result.distance / 200.0;

            colorAccum += float3(1.0 / (smoothSteps / 3));
        }

        fogAccum += min(result.distance / 1000, 1);
    }

    outputImage[pixelCoords] = float4(colorAccum / float(samplesPerPixel) + fogAccum * 0.5 / float(samplesPerPixel) , 1.0);
}
