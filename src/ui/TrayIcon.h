/**
 * Clarity Layer - System Tray Icon
 * Provides tray icon and context menu for the application
 */

#pragma once

#include <Windows.h>
#include <shellapi.h>
#include <string>

namespace clarity {

// Forward declaration
class Controller;

/**
 * TrayIcon manages the system tray icon and its context menu.
 *
 * Provides:
 * - Tray icon with tooltip showing current state
 * - Context menu for quick access to features
 * - Double-click to open settings
 */
class TrayIcon {
public:
    TrayIcon();
    ~TrayIcon();

    // Non-copyable
    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    // Initialize tray icon
    bool Initialize(HWND hwnd, HINSTANCE hInstance, Controller* controller);

    // Update tray icon and tooltip based on state
    void UpdateState();

    // Show balloon notification
    void ShowBalloon(const std::wstring& title, const std::wstring& message,
                     DWORD iconType = NIIF_INFO);

    // Handle window messages (called from main WndProc)
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // Custom message ID for tray icon
    static constexpr UINT WM_TRAYICON = WM_USER + 1;

private:
    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    Controller* m_controller = nullptr;

    NOTIFYICONDATAW m_nid = {};
    HMENU m_contextMenu = nullptr;
    bool m_initialized = false;

    // Menu item IDs
    enum class MenuItem {
        ToggleEnhancement = 1001,
        ToggleMagnifier,
        SpeakFocus,
        Profile1,
        Profile2,
        Profile3,
        OpenSettings,
        PanicOff,
        Exit
    };

    // Create context menu
    void CreateContextMenu();

    // Update menu check states
    void UpdateMenuState();

    // Show context menu at cursor
    void ShowContextMenu();

    // Handle menu command
    void HandleMenuCommand(MenuItem item);

    // Get tooltip text based on state
    std::wstring GetTooltipText();
};

} // namespace clarity
