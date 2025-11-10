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

[shader("compute")]
[numthreads(16, 16, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {
    uint2 pixelCoords = dispatchThreadID.xy;
    uint width, height;
    outputImage.GetDimensions(width, height);

    float aspectRatio = float(width) / float(height);

    float3 rayOrigin = float3(0.0, 0.0, frameUniforms.time * 0.4);
    float3 rayDirection = normalize(float3(
        (float(pixelCoords.x) / float(width) - 0.5) * 2.0 * aspectRatio,
        (float(pixelCoords.y) / float(height) - 0.5) * 2.0,
        1.0
    ));

    if (pixelCoords.x >= width || pixelCoords.y >= height) {
        return;
    }

    raymarchResult result = raymarch(rayOrigin, rayDirection, 500, 100000.0, 0.01, frameUniforms.time);

    if (!result.hit) {
        outputImage[pixelCoords] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float smoothSteps = float(result.steps) + result.lastStepSize / 0.01;
    smoothSteps = pow(smoothSteps, 0.7);
    //float distanceFade = result.distance / 200.0;

    outputImage[pixelCoords] = float4(float3(1.0 / (smoothSteps / 3)), 1.0);
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