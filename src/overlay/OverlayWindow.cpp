/**
 * Clarity Layer - Overlay Window Implementation
 */

#include "OverlayWindow.h"
#include "util/Logger.h"

#include <dwmapi.h>
#include <d3dcompiler.h>
#include <fstream>
#include <filesystem>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dwmapi.lib")

namespace clarity {

// Vertex structure for fullscreen quad
struct Vertex {
    float x, y;     // Position
    float u, v;     // Texture coordinates
};

constexpr wchar_t OVERLAY_CLASS_NAME[] = L"ClarityOverlayWindow";

OverlayWindow::OverlayWindow() = default;

OverlayWindow::~OverlayWindow() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

bool OverlayWindow::Initialize(HINSTANCE hInstance, const std::wstring& shadersPath) {
    m_hInstance = hInstance;
    m_shadersPath = shadersPath;

    // Calculate total desktop bounds
    CalculateBounds();

    // Initialize D3D11 first
    if (!InitializeD3D()) {
        LOG_ERROR("Failed to initialize D3D11");
        return false;
    }

    // Register and create window
    if (!RegisterWindowClass()) {
        LOG_ERROR("Failed to register overlay window class");
        return false;
    }

    if (!CreateOverlayWindow()) {
        LOG_ERROR("Failed to create overlay window");
        return false;
    }

    // Create render resources
    if (!CreateRenderResources()) {
        LOG_ERROR("Failed to create render resources");
        return false;
    }

    LOG_INFO("Overlay window initialized: {}x{} at ({}, {})",
             m_bounds.right - m_bounds.left,
             m_bounds.bottom - m_bounds.top,
             m_bounds.left, m_bounds.top);

    return true;
}

void OverlayWindow::Show() {
    if (!m_hwnd || m_visible) return;

    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    m_visible = true;

    LOG_DEBUG("Overlay shown");
}

void OverlayWindow::Hide() {
    if (!m_hwnd || !m_visible) return;

    ShowWindow(m_hwnd, SW_HIDE);
    m_visible = false;

    LOG_DEBUG("Overlay hidden");
}

void OverlayWindow::RenderFrame(ID3D11Texture2D* texture) {
    if (!m_visible || !texture) return;

    // Set render target
    m_context->OMSetRenderTargets(1, m_renderTarget.GetAddressOf(), nullptr);

    // Set viewport
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(m_bounds.right - m_bounds.left);
    viewport.Height = static_cast<float>(m_bounds.bottom - m_bounds.top);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);

    // Clear to transparent black
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_context->ClearRenderTargetView(m_renderTarget.Get(), clearColor);

    // Create shader resource view from texture
    D3D11_TEXTURE2D_DESC texDesc;
    texture->GetDesc(&texDesc);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    ComPtr<ID3D11ShaderResourceView> srv;
    HRESULT hr = m_device->CreateShaderResourceView(texture, &srvDesc, &srv);
    if (FAILED(hr)) {
        LOG_WARN("Failed to create SRV for frame: 0x{:08X}", hr);
        return;
    }

    // Set texture
    m_context->PSSetShaderResources(0, 1, srv.GetAddressOf());
    m_context->PSSetSamplers(0, 1, m_sampler.GetAddressOf());

    // Set blend state
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_context->OMSetBlendState(m_blendState.Get(), blendFactor, 0xFFFFFFFF);

    // Set shaders
    if (m_vertexShader) {
        m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    }
    if (m_pixelShader) {
        m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    }

    // Draw fullscreen quad
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    m_context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetInputLayout(m_inputLayout.Get());

    m_context->DrawIndexed(6, 0, 0);
}

void OverlayWindow::Present() {
    if (!m_visible || !m_swapChain) return;

    // Present with no vsync for lowest latency (like ShaderGlass)
    // Use DXGI_PRESENT_ALLOW_TEARING when supported for tear-free low-latency
    UINT syncInterval = 0;  // No vsync - don't block
    UINT presentFlags = 0;
    if (m_allowTearing) {
        presentFlags = DXGI_PRESENT_ALLOW_TEARING;
    }

    HRESULT hr = m_swapChain->Present(syncInterval, presentFlags);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        // Get detailed removal reason
        HRESULT reason = S_OK;
        if (m_device) {
            reason = m_device->GetDeviceRemovedReason();
        }
        LOG_ERROR("D3D device lost: 0x{:08X}, reason: 0x{:08X}", hr, reason);

        m_deviceLost = true;

        // Attempt recovery
        if (RecoverFromDeviceLost()) {
            LOG_INFO("Successfully recovered from device lost");
            m_deviceLost = false;

            // Notify controller to reinitialize dependent resources
            if (m_deviceLostCallback) {
                m_deviceLostCallback();
            }
        }
        else {
            LOG_ERROR("Failed to recover from device lost - hiding overlay");
            Hide();
        }
    }
}

bool OverlayWindow::RecoverFromDeviceLost() {
    LOG_INFO("Attempting D3D device recovery...");

    // Release all D3D resources
    m_renderTarget.Reset();
    m_swapChain.Reset();
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
    m_inputLayout.Reset();
    m_sampler.Reset();
    m_blendState.Reset();
    m_context.Reset();
    m_device.Reset();

    // Wait for GPU to recover
    Sleep(100);

    // Reinitialize D3D
    if (!InitializeD3D()) {
        LOG_ERROR("Failed to reinitialize D3D11 device");
        return false;
    }

    // Recreate render resources
    if (!CreateRenderResources()) {
        LOG_ERROR("Failed to recreate render resources");
        return false;
    }

    LOG_INFO("D3D device recovery complete");
    return true;
}

void OverlayWindow::OnDisplayChange() {
    LOG_INFO("Overlay handling display change");

    // Recalculate bounds
    CalculateBounds();

    // Resize window
    if (m_hwnd) {
        SetWindowPos(m_hwnd, HWND_TOPMOST,
                     m_bounds.left, m_bounds.top,
                     m_bounds.right - m_bounds.left,
                     m_bounds.bottom - m_bounds.top,
                     SWP_NOACTIVATE);
    }

    // Resize D3D resources
    Resize();
}

void OverlayWindow::OnDpiChange(UINT dpi) {
    (void)dpi;
    // DPI changes are handled by display change for overlay
    OnDisplayChange();
}

bool OverlayWindow::RegisterWindowClass() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = m_hInstance;
    wc.lpszClassName = OVERLAY_CLASS_NAME;
    wc.hCursor = nullptr;  // No cursor for overlay
    wc.hbrBackground = nullptr;

    return RegisterClassExW(&wc) != 0;
}

bool OverlayWindow::CreateOverlayWindow() {
    // Extended styles for click-through overlay
    DWORD exStyle = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST |
                    WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;

    m_hwnd = CreateWindowExW(
        exStyle,
        OVERLAY_CLASS_NAME,
        L"Clarity Overlay",
        WS_POPUP,
        m_bounds.left, m_bounds.top,
        m_bounds.right - m_bounds.left,
        m_bounds.bottom - m_bounds.top,
        nullptr, nullptr, m_hInstance, this);

    if (!m_hwnd) {
        LOG_ERROR("CreateWindowExW failed: {}", GetLastError());
        return false;
    }

    // Set layered window attributes for click-through
    // Using per-pixel alpha from D3D
    SetLayeredWindowAttributes(m_hwnd, 0, 255, LWA_ALPHA);

    // Exclude from capture to avoid feedback loop
    // This is critical - without it, we'd capture our own overlay
    BOOL affinity = TRUE;
    SetWindowDisplayAffinity(m_hwnd, WDA_EXCLUDEFROMCAPTURE);

    // Extend frame for DWM composition (allows per-pixel alpha)
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(m_hwnd, &margins);

    // Enable blur behind for composition
    DWM_BLURBEHIND bb = {};
    bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.fEnable = TRUE;
    bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
    DwmEnableBlurBehindWindow(m_hwnd, &bb);
    DeleteObject(bb.hRgnBlur);

    return true;
}

bool OverlayWindow::InitializeD3D() {
    // Create D3D11 device and context
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    #ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
    #endif

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr,                    // Default adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &m_device,
        &featureLevel,
        &m_context);

    if (FAILED(hr)) {
        LOG_ERROR("D3D11CreateDevice failed: 0x{:08X}", hr);
        return false;
    }

    // Check for tearing support (allows low-latency present without vsync)
    ComPtr<IDXGIDevice1> dxgiDevice;
    m_device.As(&dxgiDevice);
    ComPtr<IDXGIAdapter> dxgiAdapter;
    dxgiDevice->GetAdapter(&dxgiAdapter);
    ComPtr<IDXGIFactory5> dxgiFactory5;
    dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory5));

    if (dxgiFactory5) {
        BOOL tearingSupport = FALSE;
        hr = dxgiFactory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                                &tearingSupport, sizeof(tearingSupport));
        m_allowTearing = SUCCEEDED(hr) && tearingSupport;
        LOG_INFO("Tearing support: {}", m_allowTearing ? "enabled" : "disabled");
    }

    LOG_INFO("D3D11 device created, feature level: 0x{:04X}", static_cast<int>(featureLevel));
    return true;
}

bool OverlayWindow::CreateRenderResources() {
    // Get DXGI factory
    ComPtr<IDXGIDevice1> dxgiDevice;
    m_device.As(&dxgiDevice);

    ComPtr<IDXGIAdapter> dxgiAdapter;
    dxgiDevice->GetAdapter(&dxgiAdapter);

    ComPtr<IDXGIFactory2> dxgiFactory;
    dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));

    // Create swap chain for overlay window
    // Use FLIP_DISCARD with tearing for lowest latency (like ShaderGlass)
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = m_bounds.right - m_bounds.left;
    swapChainDesc.Height = m_bounds.bottom - m_bounds.top;
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 3;  // Triple buffering for smoother frames
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;  // Fastest discard mode
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    swapChainDesc.Flags = m_allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    HRESULT hr = dxgiFactory->CreateSwapChainForComposition(
        m_device.Get(),
        &swapChainDesc,
        nullptr,
        &m_swapChain);

    if (FAILED(hr)) {
        // Fall back to HWND swap chain
        hr = dxgiFactory->CreateSwapChainForHwnd(
            m_device.Get(),
            m_hwnd,
            &swapChainDesc,
            nullptr, nullptr,
            &m_swapChain);
    }

    if (FAILED(hr)) {
        LOG_ERROR("Failed to create swap chain: 0x{:08X}", hr);
        return false;
    }

    // Create render target view
    ComPtr<ID3D11Texture2D> backBuffer;
    hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get back buffer: 0x{:08X}", hr);
        return false;
    }

    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTarget);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create render target view: 0x{:08X}", hr);
        return false;
    }

    // Create fullscreen quad vertices
    Vertex vertices[] = {
        { -1.0f,  1.0f, 0.0f, 0.0f },  // Top-left
        {  1.0f,  1.0f, 1.0f, 0.0f },  // Top-right
        {  1.0f, -1.0f, 1.0f, 1.0f },  // Bottom-right
        { -1.0f, -1.0f, 0.0f, 1.0f },  // Bottom-left
    };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices;

    hr = m_device->CreateBuffer(&vbDesc, &vbData, &m_vertexBuffer);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create vertex buffer: 0x{:08X}", hr);
        return false;
    }

    // Create index buffer
    uint16_t indices[] = { 0, 1, 2, 0, 2, 3 };

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(indices);
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices;

    hr = m_device->CreateBuffer(&ibDesc, &ibData, &m_indexBuffer);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create index buffer: 0x{:08X}", hr);
        return false;
    }

    // Create sampler state
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

    hr = m_device->CreateSamplerState(&samplerDesc, &m_sampler);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create sampler state: 0x{:08X}", hr);
        return false;
    }

    // Create blend state for transparency
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = m_device->CreateBlendState(&blendDesc, &m_blendState);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create blend state: 0x{:08X}", hr);
        return false;
    }

    // Load shaders if path is provided
    if (!m_shadersPath.empty()) {
        // Load vertex shader
        auto vsPath = std::filesystem::path(m_shadersPath) / L"fullscreen_vs.cso";
        std::ifstream vsFile(vsPath, std::ios::binary);
        if (vsFile.is_open()) {
            std::vector<char> vsData((std::istreambuf_iterator<char>(vsFile)),
                                      std::istreambuf_iterator<char>());
            vsFile.close();

            hr = m_device->CreateVertexShader(vsData.data(), vsData.size(), nullptr, &m_vertexShader);
            if (FAILED(hr)) {
                LOG_WARN("Failed to create overlay vertex shader: 0x{:08X}", hr);
            }

            // Recreate input layout with shader bytecode
            D3D11_INPUT_ELEMENT_DESC layout[] = {
                { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            };

            m_inputLayout.Reset();
            hr = m_device->CreateInputLayout(layout, 2, vsData.data(), vsData.size(), &m_inputLayout);
            if (FAILED(hr)) {
                LOG_WARN("Failed to create overlay input layout: 0x{:08X}", hr);
            }
        }
        else {
            LOG_WARN("Could not open vertex shader: {}", vsPath.string());
        }

        // Load passthrough pixel shader
        auto psPath = std::filesystem::path(m_shadersPath) / L"passthrough.cso";
        std::ifstream psFile(psPath, std::ios::binary);
        if (psFile.is_open()) {
            std::vector<char> psData((std::istreambuf_iterator<char>(psFile)),
                                      std::istreambuf_iterator<char>());
            psFile.close();

            hr = m_device->CreatePixelShader(psData.data(), psData.size(), nullptr, &m_pixelShader);
            if (FAILED(hr)) {
                LOG_WARN("Failed to create overlay pixel shader: 0x{:08X}", hr);
            }
        }
        else {
            LOG_WARN("Could not open pixel shader: {}", psPath.string());
        }
    }

    return true;
}

void OverlayWindow::CalculateBounds() {
    // Get virtual screen bounds (all monitors combined)
    m_bounds.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    m_bounds.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    m_bounds.right = m_bounds.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    m_bounds.bottom = m_bounds.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);

    LOG_DEBUG("Desktop bounds: ({}, {}) - ({}, {})",
              m_bounds.left, m_bounds.top, m_bounds.right, m_bounds.bottom);
}

void OverlayWindow::Resize() {
    if (!m_swapChain) return;

    // Release old render target
    m_renderTarget.Reset();

    // Resize swap chain
    int width = m_bounds.right - m_bounds.left;
    int height = m_bounds.bottom - m_bounds.top;

    // Validate dimensions
    if (width <= 0 || height <= 0) {
        LOG_ERROR("Invalid resize dimensions: {}x{}", width, height);
        return;
    }

    UINT swapChainFlags = m_allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    HRESULT hr = m_swapChain->ResizeBuffers(3, width, height,
                                             DXGI_FORMAT_B8G8R8A8_UNORM, swapChainFlags);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to resize swap chain: 0x{:08X}", hr);
        // Hide overlay since we can't render without a valid swap chain
        Hide();
        return;
    }

    // Recreate render target
    ComPtr<ID3D11Texture2D> backBuffer;
    hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr) || !backBuffer) {
        LOG_ERROR("Failed to get back buffer after resize: 0x{:08X}", hr);
        Hide();
        return;
    }

    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTarget);
    if (FAILED(hr) || !m_renderTarget) {
        LOG_ERROR("Failed to create render target after resize: 0x{:08X}", hr);
        Hide();
        return;
    }

    LOG_DEBUG("Overlay resized to {}x{}", width, height);
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // The overlay window shouldn't handle any input - it's click-through
    // Just pass everything to DefWindowProc
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace clarity
