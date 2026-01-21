/**
 * Clarity Layer - Windows Accessibility Application
 * Main entry point
 */

#include <Windows.h>
#include <unknwn.h>  // Required before C++/WinRT headers for classic COM
#include <winrt/base.h>
#include <shellapi.h>
#include <commctrl.h>

#include "core/Controller.h"
#include "util/Logger.h"
#include "util/SafeMode.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Global controller instance
static std::unique_ptr<clarity::Controller> g_controller;

// Window class name
constexpr wchar_t WINDOW_CLASS_NAME[] = L"ClarityLayerMain";

// Message handler for main (hidden) window
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_HOTKEY:
            if (g_controller) {
                g_controller->HandleHotkey(static_cast<int>(wParam));
            }
            return 0;

        case WM_DISPLAYCHANGE:
            // Monitor configuration changed
            if (g_controller) {
                g_controller->OnDisplayChange();
            }
            return 0;

        case WM_DPICHANGED:
            // DPI changed for this window
            if (g_controller) {
                g_controller->OnDpiChange(LOWORD(wParam));
            }
            return 0;

        case WM_POWERBROADCAST:
            if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
                // System resumed from sleep
                if (g_controller) {
                    g_controller->OnSystemResume();
                }
            }
            return TRUE;

        case WM_QUERYENDSESSION:
            // System is shutting down - save state
            if (g_controller) {
                g_controller->SaveState();
            }
            return TRUE;

        case WM_ENDSESSION:
            if (wParam) {
                // Session is ending - cleanup
                if (g_controller) {
                    g_controller->Shutdown();
                }
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            // Handle custom messages from tray icon
            if (g_controller && msg >= WM_USER) {
                return g_controller->HandleCustomMessage(msg, wParam, lParam);
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// Register main window class
bool RegisterMainWindowClass(HINSTANCE hInstance) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);

    return RegisterClassExW(&wc) != 0;
}

// Create hidden main window for message handling
HWND CreateMainWindow(HINSTANCE hInstance) {
    return CreateWindowExW(
        0,
        WINDOW_CLASS_NAME,
        L"Clarity Layer",
        WS_OVERLAPPED,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1, 1,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );
}

// Set process DPI awareness
void SetDpiAwareness() {
    // Try Windows 10 1703+ API first
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using SetProcessDpiAwarenessContextFunc = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        auto setDpiAwareness = reinterpret_cast<SetProcessDpiAwarenessContextFunc>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));

        if (setDpiAwareness) {
            if (setDpiAwareness(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
                return;
            }
        }
    }

    // Fallback to Windows 8.1 API
    HMODULE shcore = LoadLibraryW(L"shcore.dll");
    if (shcore) {
        using SetProcessDpiAwarenessFunc = HRESULT(WINAPI*)(int);
        auto setDpiAwareness = reinterpret_cast<SetProcessDpiAwarenessFunc>(
            GetProcAddress(shcore, "SetProcessDpiAwareness"));

        if (setDpiAwareness) {
            setDpiAwareness(2); // PROCESS_PER_MONITOR_DPI_AWARE
        }
        FreeLibrary(shcore);
    }
}

// Check for existing instance
bool IsAlreadyRunning() {
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"ClarityLayerSingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (mutex) CloseHandle(mutex);
        return true;
    }
    // Don't close mutex - keep it alive for lifetime of process
    return false;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow) {
    // Check for single instance
    if (IsAlreadyRunning()) {
        MessageBoxW(nullptr,
            L"Clarity Layer is already running.\n\nCheck the system tray for the existing instance.",
            L"Clarity Layer",
            MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    // Set DPI awareness before any window creation
    SetDpiAwareness();

    // Initialize COM for WinRT and UI Automation
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    // Initialize common controls
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    // Initialize logger
    clarity::Logger::Initialize();
    LOG_INFO("Clarity Layer starting...");

    // Check for safe mode (Shift held during startup)
    bool safeMode = clarity::SafeMode::CheckStartupSafeMode();
    if (safeMode) {
        LOG_INFO("Safe mode activated (Shift key held)");
        MessageBoxW(nullptr,
            L"Clarity Layer is starting in Safe Mode.\n\n"
            L"All visual effects are disabled. Use the system tray to configure settings.",
            L"Clarity Layer - Safe Mode",
            MB_OK | MB_ICONINFORMATION);
    }

    // Register window class
    if (!RegisterMainWindowClass(hInstance)) {
        LOG_ERROR("Failed to register window class");
        return 1;
    }

    // Create hidden main window
    HWND hwnd = CreateMainWindow(hInstance);
    if (!hwnd) {
        LOG_ERROR("Failed to create main window");
        return 1;
    }

    // Create and initialize controller
    try {
        g_controller = std::make_unique<clarity::Controller>(hwnd, hInstance);

        if (!g_controller->Initialize(safeMode)) {
            LOG_ERROR("Failed to initialize controller");
            MessageBoxW(nullptr,
                L"Failed to initialize Clarity Layer.\n\n"
                L"Please check the log file for details.",
                L"Clarity Layer - Error",
                MB_OK | MB_ICONERROR);
            return 1;
        }

        LOG_INFO("Controller initialized successfully");

        // Show Settings window on startup (main UI)
        g_controller->ShowSettings();
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception during initialization: {}", e.what());
        MessageBoxW(nullptr,
            L"An error occurred during initialization.\n\n"
            L"Please check the log file for details.",
            L"Clarity Layer - Error",
            MB_OK | MB_ICONERROR);
        return 1;
    }

    LOG_INFO("Entering message loop");

    // Main message loop
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Cleanup
    LOG_INFO("Shutting down...");

    if (g_controller) {
        g_controller->Shutdown();
        g_controller.reset();
    }

    clarity::Logger::Shutdown();
    winrt::uninit_apartment();

    return static_cast<int>(msg.wParam);
}
