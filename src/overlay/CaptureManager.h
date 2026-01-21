/**
 * Clarity Layer - Capture Manager
 * Windows.Graphics.Capture API wrapper for screen capture
 */

#pragma once

#include <Windows.h>
#include <unknwn.h>  // Required before C++/WinRT headers for classic COM
#include <d3d11.h>
#include <dxgi1_2.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#include <functional>
#include <vector>
#include <mutex>
#include <atomic>

namespace clarity {

/**
 * Monitor information
 */
struct MonitorInfo {
    HMONITOR handle;
    RECT bounds;
    bool isPrimary;
    std::wstring name;
    UINT dpi;
};

/**
 * CaptureManager handles desktop capture using Windows.Graphics.Capture API.
 *
 * Supports:
 * - Multi-monitor capture
 * - Desktop Duplication API fallback for older systems
 * - Frame callbacks for processing
 */
class CaptureManager {
public:
    CaptureManager();
    ~CaptureManager();

    // Non-copyable
    CaptureManager(const CaptureManager&) = delete;
    CaptureManager& operator=(const CaptureManager&) = delete;

    // Initialize with D3D11 device
    bool Initialize(ID3D11Device* device);

    // Start capturing all monitors
    bool Start();

    // Stop capturing
    void Stop();

    // Restart capture (e.g., after display change)
    void Restart();

    // Set frame callback
    using FrameCallback = std::function<void(ID3D11Texture2D*)>;
    void SetFrameCallback(FrameCallback callback);

    // Get list of monitors
    const std::vector<MonitorInfo>& GetMonitors() const { return m_monitors; }

    // Handle display configuration change
    void OnDisplayChange();

    // Check if capture is running
    bool IsRunning() const { return m_running; }

    // Check if Windows.Graphics.Capture is available
    static bool IsGraphicsCaptureAvailable();

private:
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;

    // Per-monitor capture sessions
    struct MonitorCapture {
        HMONITOR monitor;
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{nullptr};
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool framePool{nullptr};
        winrt::Windows::Graphics::Capture::GraphicsCaptureSession session{nullptr};
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice winrtDevice{nullptr};
        winrt::event_token frameArrivedToken;
    };

    std::vector<MonitorCapture> m_captures;
    std::vector<MonitorInfo> m_monitors;

    FrameCallback m_frameCallback;
    std::mutex m_callbackMutex;
    std::atomic<bool> m_running{false};

    // Enumerate monitors
    void EnumerateMonitors();
    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdc, LPRECT lprcMonitor, LPARAM dwData);

    // Create capture for a monitor
    bool CreateCaptureForMonitor(HMONITOR monitor);

    // Frame arrived handler
    void OnFrameArrived(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
        winrt::Windows::Foundation::IInspectable const& args);

    // Create WinRT device from D3D11 device
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice CreateWinRTDevice();

    // Get D3D11 texture from WinRT surface
    ID3D11Texture2D* GetTextureFromSurface(
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface const& surface);
};

} // namespace clarity
