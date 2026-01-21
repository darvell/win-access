/**
 * Clarity Layer - Lens Renderer
 * Renders magnified lens overlay
 */

#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <string>

namespace clarity {

using Microsoft::WRL::ComPtr;

/**
 * LensRenderer draws a magnified lens at a specified position.
 * Used when lens mode is enabled instead of full-screen magnification.
 */
class LensRenderer {
public:
    LensRenderer();
    ~LensRenderer();

    // Initialize with D3D device
    bool Initialize(ID3D11Device* device, const std::wstring& shadersPath);

    // Render lens at position
    void Render(ID3D11Texture2D* source,
                ID3D11RenderTargetView* target,
                POINT center,
                float zoomLevel,
                int lensSize);

    // Set lens shape (circular or rectangular)
    enum class Shape { Circular, Rectangular };
    void SetShape(Shape shape) { m_shape = shape; }
    Shape GetShape() const { return m_shape; }

    // Set border appearance
    void SetBorderColor(float r, float g, float b, float a);
    void SetBorderWidth(float width);

private:
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;

    Shape m_shape = Shape::Circular;
    float m_borderColor[4] = { 1.0f, 1.0f, 0.0f, 1.0f };  // Yellow
    float m_borderWidth = 3.0f;

    // Lens shader
    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_lensShader;
    ComPtr<ID3D11InputLayout> m_inputLayout;

    // Constant buffers
    ComPtr<ID3D11Buffer> m_lensParamsBuffer;
    ComPtr<ID3D11Buffer> m_vertexBuffer;
    ComPtr<ID3D11Buffer> m_indexBuffer;
    ComPtr<ID3D11SamplerState> m_sampler;

    // Lens parameters struct (matches HLSL)
    struct LensParams {
        float centerU, centerV;
        float radiusU, radiusV;
        float zoomLevel;
        float borderWidth;
        float borderR, borderG, borderB, borderA;
        int lensShape;
        float padding[1];
    };
};

} // namespace clarity
