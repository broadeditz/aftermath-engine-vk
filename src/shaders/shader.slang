import raymarch;
import sdf;

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
};

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

// Generate random point in unit disk
float2 randomInUnitDisk(float2 seed) {
    float angle = hash(seed) * 6.28318530718; // 2 * PI
    float radius = sqrt(hash(seed + 0.1));
    return float2(cos(angle), sin(angle)) * radius;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {
    uint2 pixelCoords = dispatchThreadID.xy;
    uint width, height;
    outputImage.GetDimensions(width, height);

    float aspectRatio = float(width) / float(height);

    float fov = 2.0;
    float focusDistance = 25.0 * abs(sin(frameUniforms.time * 0.5)) + 10;
    float aperture = 0.1;

    int samplesPerPixel = 1;

    float3 colorAccum = float3(0.0, 0.0, 0.0);
    int hitSamples = 0;

    // Calculate position on the image plane (focal plane)
    float2 uv = float2(
        (float(pixelCoords.x) / float(width) - 0.5) * 2.0 * aspectRatio,
        (float(pixelCoords.y) / float(height) - 0.5) * 2.0
    );

    float3 cameraPosition = float3(0.0, 0.0, frameUniforms.time * 0.4);

    // Point on the image plane (determines FOV)
    float3 imagePoint = float3(uv.x, uv.y, fov) + cameraPosition;

    float3 centerDirection = normalize(imagePoint - cameraPosition);

    // Focal point at the desired focus distance (this is where all rays should converge)
    float3 focalPoint = cameraPosition + centerDirection * focusDistance;

    for (int i = 0; i < samplesPerPixel; i++) {
        // apply aperture offset to ray origin
        float2 seed = float2(pixelCoords) + float2(i * 123.45) + frameUniforms.time * 1000.0;
        float2 apertureOffset = randomInUnitDisk(seed) * aperture;
        float3 rayOrigin = cameraPosition + float3(apertureOffset.x, apertureOffset.y, 0.0);

        // Final ray direction: from offset origin TO the fixed focal point
        float3 rayDirection = normalize(focalPoint - rayOrigin);

        if (pixelCoords.x >= width || pixelCoords.y >= height) {
            return;
        }

        raymarchResult result = raymarch(rayOrigin, rayDirection, 500, 100000.0, 0.01, frameUniforms.time);

        if (result.hit) {
            float smoothSteps = float(result.steps) + result.lastStepSize / 0.01;
            smoothSteps = pow(smoothSteps, 0.7);
            // float distanceFade = result.distance / 200.0;

            colorAccum += float3(1.0 / (smoothSteps / 3));
            hitSamples++;
        }
    }

    outputImage[pixelCoords] = float4(colorAccum / float(samplesPerPixel), 1.0);
}

// foo

/*[shader("compute")]
[numthreads(16, 16, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {
    uint2 pixelCoords = dispatchThreadID.xy;
    uint width, height;
    outputImage.GetDimensions(width, height);

    if (pixelCoords.x >= width || pixelCoords.y >= height) {
        return;
    }

    // Output pixel coordinates as color (normalized)
    float3 color = float3(
        float(pixelCoords.x) / float(width),
        float(pixelCoords.y) / float(height),
        0.0
    );

    outputImage[pixelCoords] = float4(color, 1.0);
}*/