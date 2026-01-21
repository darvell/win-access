/**
 * Clarity Layer - Capture Manager Implementation
 */

#include "CaptureManager.h"
#include "util/Logger.h"

#include <inspectable.h>
#include <dxgi.h>
#include <ShellScalingApi.h>

namespace clarity {

CaptureManager::CaptureManager() = default;

CaptureManager::~CaptureManager() {
    Stop();
}

bool CaptureManager::Initialize(ID3D11Device* device) {
    if (!device) {
        LOG_ERROR("CaptureManager::Initialize - null device");
        return false;
    }

    m_device = device;
    m_device->GetImmediateContext(&m_context);

    // Enumerate monitors
    EnumerateMonitors();

    LOG_INFO("CaptureManager initialized with {} monitors", m_monitors.size());
    return true;
}

bool CaptureManager::Start() {
    if (m_running) {
        LOG_WARN("CaptureManager already running");
        return true;
    }

    if (!IsGraphicsCaptureAvailable()) {
        LOG_ERROR("Windows.Graphics.Capture API not available");
        return false;
    }

    LOG_INFO("Starting capture for {} monitors", m_monitors.size());

    // Create capture for each monitor
    for (const auto& monitor : m_monitors) {
        if (!CreateCaptureForMonitor(monitor.handle)) {
            LOG_ERROR("Failed to create capture for monitor");
            // Continue with other monitors
        }
    }

    m_running = true;
    LOG_INFO("Capture started");
    return !m_captures.empty();
}

void CaptureManager::Stop() {
    if (!m_running) return;

    LOG_INFO("Stopping capture");

    for (auto& capture : m_captures) {
        // Revoke the event handler first (before closing frame pool)
        if (capture.framePool && capture.frameArrivedToken) {
            capture.framePool.FrameArrived(capture.frameArrivedToken);
            capture.frameArrivedToken = {};  // Reset token after revocation
        }
        if (capture.session) {
            capture.session.Close();
            capture.session = nullptr;
        }
        if (capture.framePool) {
            capture.framePool.Close();
            capture.framePool = nullptr;
        }
        capture.item = nullptr;
    }

    m_captures.clear();
    m_running = false;

    LOG_INFO("Capture stopped");
}

void CaptureManager::Restart() {
    LOG_INFO("Restarting capture");
    Stop();

    // Re-enumerate monitors (may have changed)
    EnumerateMonitors();

    Start();
}

void CaptureManager::SetFrameCallback(FrameCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_frameCallback = std::move(callback);
}

void CaptureManager::OnDisplayChange() {
    LOG_INFO("Display configuration changed");
    Restart();
}

bool CaptureManager::IsGraphicsCaptureAvailable() {
    // Windows.Graphics.Capture requires Windows 10 1903+
    // Check by trying to get the interop factory
    winrt::com_ptr<IGraphicsCaptureItemInterop> interop;
    HRESULT hr = winrt::get_activation_factory<
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem,
        IGraphicsCaptureItemInterop>()->QueryInterface(IID_PPV_ARGS(interop.put()));

    return SUCCEEDED(hr);
}

void CaptureManager::EnumerateMonitors() {
    m_monitors.clear();

    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(this));

    LOG_INFO("Enumerated {} monitors", m_monitors.size());
    for (const auto& monitor : m_monitors) {
        LOG_DEBUG("  Monitor: {} x {} at ({}, {}), DPI={}, Primary={}",
                  monitor.bounds.right - monitor.bounds.left,
                  monitor.bounds.bottom - monitor.bounds.top,
                  monitor.bounds.left, monitor.bounds.top,
                  monitor.dpi, monitor.isPrimary);
    }
}

BOOL CALLBACK CaptureManager::MonitorEnumProc(HMONITOR hMonitor, HDC hdc, LPRECT lprcMonitor, LPARAM dwData) {
    (void)hdc;

    auto* self = reinterpret_cast<CaptureManager*>(dwData);

    MONITORINFOEXW monitorInfo = {};
    monitorInfo.cbSize = sizeof(MONITORINFOEXW);
    if (!GetMonitorInfoW(hMonitor, &monitorInfo)) {
        return TRUE;  // Continue enumeration
    }

    MonitorInfo info;
    info.handle = hMonitor;
    info.bounds = *lprcMonitor;
    info.isPrimary = (monitorInfo.dwFlags & MONITORINFOF_PRIMARY) != 0;
    info.name = monitorInfo.szDevice;

    // Get DPI
    UINT dpiX, dpiY;
    if (SUCCEEDED(GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        info.dpi = dpiX;
    }
    else {
        info.dpi = 96;  // Default
    }

    self->m_monitors.push_back(info);
    return TRUE;
}

bool CaptureManager::CreateCaptureForMonitor(HMONITOR monitor) {
    try {
        MonitorCapture capture;
        capture.monitor = monitor;

        // Create WinRT device
        capture.winrtDevice = CreateWinRTDevice();
        if (!capture.winrtDevice) {
            LOG_ERROR("Failed to create WinRT device");
            return false;
        }

        // Create capture item for monitor
        auto interop = winrt::get_activation_factory<
            winrt::Windows::Graphics::Capture::GraphicsCaptureItem,
            IGraphicsCaptureItemInterop>();

        winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{nullptr};
        HRESULT hr = interop->CreateForMonitor(
            monitor,
            winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
            winrt::put_abi(item));

        if (FAILED(hr) || !item) {
            LOG_ERROR("Failed to create capture item for monitor: 0x{:08X}", hr);
            return false;
        }

        capture.item = item;

        // Get size
        auto size = item.Size();

        // Create frame pool
        capture.framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
            capture.winrtDevice,
            winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,  // Number of buffers
            size);

        if (!capture.framePool) {
            LOG_ERROR("Failed to create frame pool");
            return false;
        }

        // Subscribe to frame arrived
        capture.frameArrivedToken = capture.framePool.FrameArrived(
            [this](auto&& sender, auto&& args) {
                OnFrameArrived(sender, args);
            });

        // Create session
        capture.session = capture.framePool.CreateCaptureSession(item);

        // Disable cursor capture (we don't want cursor in our overlay)
        capture.session.IsCursorCaptureEnabled(false);

        // Disable border (Windows 11 feature)
        try {
            // This may fail on older Windows versions
            capture.session.IsBorderRequired(false);
        }
        catch (...) {
            // Ignore - not available on this Windows version
        }

        // Start capture
        capture.session.StartCapture();

        m_captures.push_back(std::move(capture));

        LOG_INFO("Created capture session for monitor");
        return true;
    }
    catch (const winrt::hresult_error& e) {
        LOG_ERROR("WinRT error creating capture: 0x{:08X} - {}",
                  static_cast<uint32_t>(e.code()),
                  winrt::to_string(e.message()));
        return false;
    }
}

void CaptureManager::OnFrameArrived(
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
    winrt::Windows::Foundation::IInspectable const& args) {
    (void)args;

    if (!m_running) return;

    try {
        auto frame = sender.TryGetNextFrame();
        if (!frame) return;

        auto surface = frame.Surface();
        if (!surface) return;

        // Get D3D11 texture from surface
        ID3D11Texture2D* texture = GetTextureFromSurface(surface);
        if (!texture) return;

        // AddRef to ensure texture lifetime during callback
        // The texture is owned by the frame, so we need to extend its lifetime
        texture->AddRef();

        // Call the frame callback
        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            if (m_frameCallback) {
                m_frameCallback(texture);
            }
        }

        // Release our reference - texture may still be used by frame until frame goes out of scope
        texture->Release();
    }
    catch (const winrt::hresult_error& e) {
        LOG_WARN("Frame processing error: 0x{:08X}", static_cast<uint32_t>(e.code()));
    }
}

winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice CaptureManager::CreateWinRTDevice() {
    // Get DXGI device from D3D11 device
    winrt::com_ptr<IDXGIDevice> dxgiDevice;
    HRESULT hr = m_device->QueryInterface(IID_PPV_ARGS(dxgiDevice.put()));
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get DXGI device: 0x{:08X}", hr);
        return nullptr;
    }

    // Create WinRT device
    winrt::com_ptr<IInspectable> inspectable;
    hr = CreateDirect3D11DeviceFromDXGIDevice(
        dxgiDevice.get(),
        inspectable.put());

    if (FAILED(hr)) {
        LOG_ERROR("Failed to create WinRT device: 0x{:08X}", hr);
        return nullptr;
    }

    return inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
}

ID3D11Texture2D* CaptureManager::GetTextureFromSurface(
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface const& surface) {

    auto access = surface.as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    if (!access) return nullptr;

    winrt::com_ptr<ID3D11Texture2D> texture;
    HRESULT hr = access->GetInterface(IID_PPV_ARGS(texture.put()));
    if (FAILED(hr)) return nullptr;

    // Return raw pointer (caller must not release)
    return texture.get();
}

} // namespace clarity
