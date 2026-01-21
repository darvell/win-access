/**
 * Clarity Layer - Overlay Window
 * Click-through DirectX 11 window for rendering effects
 */

#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <vector>

namespace clarity {

using Microsoft::WRL::ComPtr;

/**
 * OverlayWindow creates a click-through, always-on-top window
 * for rendering visual effects over the desktop.
 *
 * Based on ShaderGlass approach:
 * - WS_EX_LAYERED | WS_EX_TRANSPARENT for click-through
 * - DirectX 11 for GPU-accelerated rendering
 * - Excludes itself from capture to avoid feedback loops
 */
class OverlayWindow {
public:
    OverlayWindow();
    ~OverlayWindow();

    // Non-copyable
    OverlayWindow(const OverlayWindow&) = delete;
    OverlayWindow& operator=(const OverlayWindow&) = delete;

    // Initialize D3D11 and create window
    bool Initialize(HINSTANCE hInstance);

    // Show/hide overlay
    void Show();
    void Hide();
    bool IsVisible() const { return m_visible; }

    // Render a frame
    void RenderFrame(ID3D11Texture2D* texture);

    // Present (swap buffers)
    void Present();

    // Get D3D11 device (for CaptureManager and ShaderPipeline)
    ID3D11Device* GetD3DDevice() { return m_device.Get(); }
    ID3D11DeviceContext* GetD3DContext() { return m_context.Get(); }

    // Handle display/DPI changes
    void OnDisplayChange();
    void OnDpiChange(UINT dpi);

    // Get window handle
    HWND GetHwnd() const { return m_hwnd; }

    // Get total overlay bounds (across all monitors)
    RECT GetBounds() const { return m_bounds; }

private:
    HINSTANCE m_hInstance = nullptr;
    HWND m_hwnd = nullptr;
    bool m_visible = false;

    // Total desktop bounds
    RECT m_bounds = {};

    // D3D11 resources
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain1> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_renderTarget;

    // Vertex/index buffers for fullscreen quad
    ComPtr<ID3D11Buffer> m_vertexBuffer;
    ComPtr<ID3D11Buffer> m_indexBuffer;
    ComPtr<ID3D11InputLayout> m_inputLayout;

    // Sampler state
    ComPtr<ID3D11SamplerState> m_sampler;

    // Blend state for transparency
    ComPtr<ID3D11BlendState> m_blendState;

    // Register window class
    bool RegisterWindowClass();

    // Create the overlay window
    bool CreateOverlayWindow();

    // Initialize D3D11
    bool InitializeD3D();

    // Create render resources
    bool CreateRenderResources();

    // Calculate total desktop bounds
    void CalculateBounds();

    // Resize swap chain
    void Resize();

    // Window procedure
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

} // namespace clarity
