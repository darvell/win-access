/**
 * Clarity Layer - Passthrough Shader
 * Simple texture copy with no modifications
 */

Texture2D inputTex : register(t0);
SamplerState sampler0 : register(s0);

struct PS_INPUT {
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_Target {
    return inputTex.Sample(sampler0, input.TexCoord);
}
