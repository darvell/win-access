/**
 * Clarity Layer - Contrast/Brightness/Gamma/Saturation Shader
 * Primary visual enhancement shader
 */

Texture2D inputTex : register(t0);
SamplerState sampler0 : register(s0);

cbuffer TransformParams : register(b0) {
    float contrast;      // 0.0 - 4.0
    float brightness;    // -1.0 - 1.0
    float gamma;         // 0.1 - 4.0
    float saturation;    // 0.0 - 2.0
    int invertMode;      // 0=off, 1=full, 2=brightness-only
    float edgeStrength;  // 0.0 - 1.0 (unused in this shader)
    float2 padding;
};

struct PS_INPUT {
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_Target {
    float4 color = inputTex.Sample(sampler0, input.TexCoord);

    // Apply contrast: (color - 0.5) * contrast + 0.5
    color.rgb = (color.rgb - 0.5f) * contrast + 0.5f;

    // Apply brightness
    color.rgb = color.rgb + brightness;

    // Apply gamma correction
    // Clamp to prevent issues with negative values before pow
    color.rgb = max(color.rgb, 0.0f);
    color.rgb = pow(color.rgb, 1.0f / gamma);

    // Apply saturation
    // Convert to grayscale using luminance weights
    float luminance = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    color.rgb = lerp(float3(luminance, luminance, luminance), color.rgb, saturation);

    // Clamp final result
    color.rgb = saturate(color.rgb);

    return color;
}
