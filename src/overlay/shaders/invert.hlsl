/**
 * Clarity Layer - Color Inversion Shader
 * Supports full inversion and brightness-only inversion
 */

Texture2D inputTex : register(t0);
SamplerState sampler0 : register(s0);

cbuffer TransformParams : register(b0) {
    float contrast;
    float brightness;
    float gamma;
    float saturation;
    int invertMode;      // 0=off, 1=full, 2=brightness-only (hue-preserving)
    float edgeStrength;
    float2 padding;
};

struct PS_INPUT {
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

// Convert RGB to HSL
float3 RGBtoHSL(float3 rgb) {
    float maxC = max(max(rgb.r, rgb.g), rgb.b);
    float minC = min(min(rgb.r, rgb.g), rgb.b);
    float delta = maxC - minC;

    float L = (maxC + minC) * 0.5f;
    float S = 0.0f;
    float H = 0.0f;

    if (delta > 0.0001f) {
        S = (L < 0.5f) ? (delta / (maxC + minC)) : (delta / (2.0f - maxC - minC));

        if (rgb.r == maxC) {
            H = (rgb.g - rgb.b) / delta + (rgb.g < rgb.b ? 6.0f : 0.0f);
        } else if (rgb.g == maxC) {
            H = (rgb.b - rgb.r) / delta + 2.0f;
        } else {
            H = (rgb.r - rgb.g) / delta + 4.0f;
        }
        H /= 6.0f;
    }

    return float3(H, S, L);
}

// Helper for HSL to RGB
float HueToRGB(float p, float q, float t) {
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
    if (t < 0.5f) return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}

// Convert HSL to RGB
float3 HSLtoRGB(float3 hsl) {
    float H = hsl.x;
    float S = hsl.y;
    float L = hsl.z;

    if (S < 0.0001f) {
        return float3(L, L, L);
    }

    float q = (L < 0.5f) ? (L * (1.0f + S)) : (L + S - L * S);
    float p = 2.0f * L - q;

    float r = HueToRGB(p, q, H + 1.0f / 3.0f);
    float g = HueToRGB(p, q, H);
    float b = HueToRGB(p, q, H - 1.0f / 3.0f);

    return float3(r, g, b);
}

float4 main(PS_INPUT input) : SV_Target {
    float4 color = inputTex.Sample(sampler0, input.TexCoord);

    if (invertMode == 1) {
        // Full color inversion
        color.rgb = 1.0f - color.rgb;
    }
    else if (invertMode == 2) {
        // Brightness-only inversion (preserves hue)
        // Convert to HSL, invert L, convert back
        float3 hsl = RGBtoHSL(color.rgb);
        hsl.z = 1.0f - hsl.z;  // Invert lightness
        color.rgb = HSLtoRGB(hsl);
    }
    // invertMode == 0: no inversion, passthrough

    return color;
}
