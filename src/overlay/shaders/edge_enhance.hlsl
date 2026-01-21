/**
 * Clarity Layer - Edge Enhancement Shader
 * Emphasizes edges and boundaries for better visibility
 */

Texture2D inputTex : register(t0);
SamplerState sampler0 : register(s0);

cbuffer TransformParams : register(b0) {
    float contrast;
    float brightness;
    float gamma;
    float saturation;
    int invertMode;
    float edgeStrength;  // 0.0 - 1.0
    float2 padding;
};

cbuffer ScreenSize : register(b1) {
    float2 screenSize;
    float2 texelSize;
};

struct PS_INPUT {
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

// Sobel edge detection
float3 SobelEdge(float2 uv, float2 texelSize) {
    // Sobel kernels
    // Gx:            Gy:
    // -1  0  1      -1 -2 -1
    // -2  0  2       0  0  0
    // -1  0  1       1  2  1

    float3 samples[9];
    float2 offsets[9] = {
        float2(-1, -1), float2(0, -1), float2(1, -1),
        float2(-1,  0), float2(0,  0), float2(1,  0),
        float2(-1,  1), float2(0,  1), float2(1,  1)
    };

    [unroll]
    for (int i = 0; i < 9; i++) {
        samples[i] = inputTex.Sample(sampler0, uv + offsets[i] * texelSize).rgb;
    }

    // Convert to grayscale for edge detection
    float gray[9];
    [unroll]
    for (int j = 0; j < 9; j++) {
        gray[j] = dot(samples[j], float3(0.2126f, 0.7152f, 0.0722f));
    }

    // Apply Sobel operators
    float gx = gray[0] * -1.0f + gray[2] * 1.0f +
               gray[3] * -2.0f + gray[5] * 2.0f +
               gray[6] * -1.0f + gray[8] * 1.0f;

    float gy = gray[0] * -1.0f + gray[1] * -2.0f + gray[2] * -1.0f +
               gray[6] *  1.0f + gray[7] *  2.0f + gray[8] *  1.0f;

    // Edge magnitude
    float edge = sqrt(gx * gx + gy * gy);

    return float3(edge, edge, edge);
}

float4 main(PS_INPUT input) : SV_Target {
    // Get screen dimensions from texture
    float width, height;
    inputTex.GetDimensions(width, height);
    float2 texelSize = float2(1.0f / width, 1.0f / height);

    float4 color = inputTex.Sample(sampler0, input.TexCoord);

    if (edgeStrength > 0.0f) {
        // Detect edges
        float3 edges = SobelEdge(input.TexCoord, texelSize);

        // Enhance edges by adding them to the original
        // Using additive blending for bright edge emphasis
        color.rgb = color.rgb + edges * edgeStrength;

        // Alternative: darken edges for outline effect
        // color.rgb = color.rgb * (1.0f - edges * edgeStrength * 0.5f);

        color.rgb = saturate(color.rgb);
    }

    return color;
}
