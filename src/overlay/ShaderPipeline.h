/**
 * Clarity Layer - Shader Pipeline
 * GPU shader-based visual transforms
 */

#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <memory>

#include "core/ProfileManager.h"  // For InvertMode

namespace clarity {

using Microsoft::WRL::ComPtr;

/**
 * Transform parameters passed to shaders
 */
struct TransformParams {
    float contrast = 1.0f;       // 0.0 - 4.0
    float brightness = 0.0f;     // -1.0 - 1.0
    float gamma = 1.0f;          // 0.1 - 4.0
    float saturation = 1.0f;     // 0.0 - 2.0
    int invertMode = 0;          // 0=off, 1=full, 2=brightness-only
    float edgeStrength = 0.0f;   // 0.0 - 1.0
    float padding[2];            // Align to 16 bytes
};

/**
 * ShaderPipeline manages GPU shaders for visual transforms.
 *
 * Applies effects like:
 * - Contrast/brightness adjustment
 * - Gamma correction
 * - Color inversion (full or brightness-only)
 * - Saturation adjustment
 * - Edge enhancement (optional)
 */
class ShaderPipeline {
public:
    ShaderPipeline();
    ~ShaderPipeline();

    // Non-copyable
    ShaderPipeline(const ShaderPipeline&) = delete;
    ShaderPipeline& operator=(const ShaderPipeline&) = delete;

    // Initialize with D3D device and path to compiled shaders
    bool Initialize(ID3D11Device* device, const std::wstring& shadersPath);

    // Process a frame through the shader pipeline
    // Returns the processed texture (owned by pipeline, valid until next Process call)
    ID3D11Texture2D* Process(ID3D11Texture2D* input);

    // Parameter setters
    void SetContrast(float contrast);
    void SetBrightness(float brightness);
    void SetGamma(float gamma);
    void SetSaturation(float saturation);
    void SetInvertMode(InvertMode mode);
    void SetEdgeStrength(float strength);

    // Apply all parameters from a profile
    void ApplyProfile(const VisualSettings& settings);

    // Force update of constant buffer
    void UpdateParameters();

    // Get current parameters
    const TransformParams& GetParams() const { return m_params; }

    // Check if pipeline is ready
    bool IsReady() const { return m_ready; }

private:
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    std::wstring m_shadersPath;
    bool m_ready = false;

    // Current transform parameters
    TransformParams m_params;
    bool m_paramsDirty = true;

    // Shaders
    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_contrastShader;
    ComPtr<ID3D11PixelShader> m_invertShader;
    ComPtr<ID3D11PixelShader> m_edgeShader;
    ComPtr<ID3D11PixelShader> m_passthroughShader;
    ComPtr<ID3D11InputLayout> m_inputLayout;

    // Constant buffer for parameters
    ComPtr<ID3D11Buffer> m_constantBuffer;

    // Intermediate textures for multi-pass rendering
    ComPtr<ID3D11Texture2D> m_intermediateTexture;
    ComPtr<ID3D11RenderTargetView> m_intermediateRTV;
    ComPtr<ID3D11ShaderResourceView> m_intermediateSRV;

    // Output texture
    ComPtr<ID3D11Texture2D> m_outputTexture;
    ComPtr<ID3D11RenderTargetView> m_outputRTV;
    ComPtr<ID3D11ShaderResourceView> m_outputSRV;

    // Current output size
    int m_outputWidth = 0;
    int m_outputHeight = 0;

    // Fullscreen quad
    ComPtr<ID3D11Buffer> m_vertexBuffer;
    ComPtr<ID3D11Buffer> m_indexBuffer;
    ComPtr<ID3D11SamplerState> m_sampler;

    // Load a compiled shader file
    bool LoadVertexShader(const std::wstring& filename);
    bool LoadPixelShader(const std::wstring& filename, ComPtr<ID3D11PixelShader>& shader);

    // Create intermediate/output textures
    bool CreateIntermediateTextures(int width, int height);

    // Create constant buffer
    bool CreateConstantBuffer();

    // Create fullscreen quad
    bool CreateFullscreenQuad();

    // Render pass helper
    void RenderPass(ID3D11ShaderResourceView* input,
                    ID3D11RenderTargetView* output,
                    ID3D11PixelShader* shader,
                    int width, int height);
};

} // namespace clarity
