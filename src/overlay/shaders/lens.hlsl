/**
 * Clarity Layer - Lens/Magnification Shader
 * Creates a magnified lens effect around a focus point
 */

Texture2D inputTex : register(t0);
SamplerState sampler0 : register(s0);

cbuffer LensParams : register(b0) {
    float2 lensCenter;   // Center of lens in UV coordinates (0-1)
    float lensRadius;    // Radius of lens in UV coordinates
    float zoomLevel;     // Magnification factor (1.0 = no zoom)
    float borderWidth;   // Width of lens border
    float4 borderColor;  // Color of lens border
    int lensShape;       // 0 = circular, 1 = rectangular
    float2 padding;
};

struct PS_INPUT {
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_Target {
    float2 uv = input.TexCoord;

    // Calculate distance from lens center
    float2 delta = uv - lensCenter;

    float dist;
    if (lensShape == 0) {
        // Circular lens
        dist = length(delta);
    } else {
        // Rectangular lens
        dist = max(abs(delta.x), abs(delta.y));
    }

    // Check if we're inside the lens
    if (dist < lensRadius) {
        // Inside lens - apply magnification
        // Map the lens area to a smaller area of the source texture

        // Calculate the zoomed UV coordinates
        float2 zoomedUV = lensCenter + delta / zoomLevel;

        // Clamp to valid texture coordinates
        zoomedUV = saturate(zoomedUV);

        // Sample the zoomed texture
        float4 color = inputTex.Sample(sampler0, zoomedUV);

        // Add subtle border at edge of lens
        float borderStart = lensRadius - borderWidth;
        if (dist > borderStart) {
            float borderFactor = (dist - borderStart) / borderWidth;
            color = lerp(color, borderColor, borderFactor * 0.5f);
        }

        return color;
    }

    // Outside lens - return original
    return inputTex.Sample(sampler0, uv);
}
