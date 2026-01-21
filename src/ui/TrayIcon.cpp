/**
 * Clarity Layer - System Tray Icon Implementation
 */

#include "TrayIcon.h"
#include "core/Controller.h"
#include "util/Logger.h"

namespace clarity {

TrayIcon::TrayIcon() = default;

TrayIcon::~TrayIcon() {
    if (m_initialized) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
    }

    if (m_contextMenu) {
        DestroyMenu(m_contextMenu);
    }
}

bool TrayIcon::Initialize(HWND hwnd, HINSTANCE hInstance, Controller* controller) {
    m_hwnd = hwnd;
    m_hInstance = hInstance;
    m_controller = controller;

    // Initialize NOTIFYICONDATA
    ZeroMemory(&m_nid, sizeof(m_nid));
    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = hwnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    m_nid.uCallbackMessage = WM_TRAYICON;

    // Load icon (use application icon or fallback to system icon)
    m_nid.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    if (!m_nid.hIcon) {
        m_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }

    // Set tooltip
    wcscpy_s(m_nid.szTip, GetTooltipText().c_str());

    // Add the tray icon
    if (!Shell_NotifyIconW(NIM_ADD, &m_nid)) {
        LOG_ERROR("Failed to add tray icon");
        return false;
    }

    // Use version 4 behavior (for balloon tips)
    m_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &m_nid);

    // Create context menu
    CreateContextMenu();

    m_initialized = true;
    LOG_INFO("TrayIcon initialized");
    return true;
}

void TrayIcon::UpdateState() {
    if (!m_initialized) return;

    // Update tooltip
    wcscpy_s(m_nid.szTip, GetTooltipText().c_str());

    // Update icon if needed (could use different icons for different states)
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);

    // Update menu check states
    UpdateMenuState();
}

void TrayIcon::ShowBalloon(const std::wstring& title, const std::wstring& message, DWORD iconType) {
    if (!m_initialized) return;

    m_nid.uFlags |= NIF_INFO;
    wcscpy_s(m_nid.szInfoTitle, title.c_str());
    wcscpy_s(m_nid.szInfo, message.c_str());
    m_nid.dwInfoFlags = iconType;

    Shell_NotifyIconW(NIM_MODIFY, &m_nid);

    // Clear the flags after showing
    m_nid.uFlags &= ~NIF_INFO;
}

LRESULT TrayIcon::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg != WM_TRAYICON) {
        return 0;
    }

    switch (LOWORD(lParam)) {
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            ShowContextMenu();
            break;

        case WM_LBUTTONDBLCLK:
            // Double-click opens settings
            HandleMenuCommand(MenuItem::OpenSettings);
            break;

        case NIN_SELECT:
        case NIN_KEYSELECT:
            // Single click - could toggle enhancement
            break;

        default:
            break;
    }

    return 0;
}

void TrayIcon::CreateContextMenu() {
    m_contextMenu = CreatePopupMenu();
    if (!m_contextMenu) return;

    // Feature toggles
    AppendMenuW(m_contextMenu, MF_STRING, static_cast<UINT_PTR>(MenuItem::ToggleEnhancement),
                L"Toggle &Enhancement\tWin+E");
    AppendMenuW(m_contextMenu, MF_STRING, static_cast<UINT_PTR>(MenuItem::ToggleMagnifier),
                L"Toggle &Magnifier\tWin+M");
    AppendMenuW(m_contextMenu, MF_STRING, static_cast<UINT_PTR>(MenuItem::SpeakFocus),
                L"&Speak Focus\tWin+F");

    AppendMenuW(m_contextMenu, MF_SEPARATOR, 0, nullptr);

    // Profiles submenu
    HMENU profileMenu = CreatePopupMenu();
    AppendMenuW(profileMenu, MF_STRING, static_cast<UINT_PTR>(MenuItem::Profile1),
                L"Profile &1\tWin+1");
    AppendMenuW(profileMenu, MF_STRING, static_cast<UINT_PTR>(MenuItem::Profile2),
                L"Profile &2\tWin+2");
    AppendMenuW(profileMenu, MF_STRING, static_cast<UINT_PTR>(MenuItem::Profile3),
                L"Profile &3\tWin+3");
    AppendMenuW(m_contextMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(profileMenu),
                L"&Profiles");

    AppendMenuW(m_contextMenu, MF_SEPARATOR, 0, nullptr);

    // Settings
    AppendMenuW(m_contextMenu, MF_STRING, static_cast<UINT_PTR>(MenuItem::OpenSettings),
                L"&Settings...");

    AppendMenuW(m_contextMenu, MF_SEPARATOR, 0, nullptr);

    // Emergency off
    AppendMenuW(m_contextMenu, MF_STRING, static_cast<UINT_PTR>(MenuItem::PanicOff),
                L"&Panic Off\tCtrl+Alt+X");

    AppendMenuW(m_contextMenu, MF_SEPARATOR, 0, nullptr);

    // Exit
    AppendMenuW(m_contextMenu, MF_STRING, static_cast<UINT_PTR>(MenuItem::Exit),
                L"E&xit");
}

void TrayIcon::UpdateMenuState() {
    if (!m_contextMenu || !m_controller) return;

    // Update check states based on controller state
    UINT enhanceFlags = m_controller->IsEnhancementEnabled() ? MF_CHECKED : MF_UNCHECKED;
    CheckMenuItem(m_contextMenu, static_cast<UINT>(MenuItem::ToggleEnhancement),
                  MF_BYCOMMAND | enhanceFlags);

    UINT magFlags = m_controller->IsMagnifierEnabled() ? MF_CHECKED : MF_UNCHECKED;
    CheckMenuItem(m_contextMenu, static_cast<UINT>(MenuItem::ToggleMagnifier),
                  MF_BYCOMMAND | magFlags);

    // Gray out features if in safe mode
    UINT enableFlags = m_controller->IsInSafeMode() ? MF_GRAYED : MF_ENABLED;
    EnableMenuItem(m_contextMenu, static_cast<UINT>(MenuItem::ToggleEnhancement),
                   MF_BYCOMMAND | enableFlags);
    EnableMenuItem(m_contextMenu, static_cast<UINT>(MenuItem::ToggleMagnifier),
                   MF_BYCOMMAND | enableFlags);
}

void TrayIcon::ShowContextMenu() {
    // Get cursor position
    POINT pt;
    GetCursorPos(&pt);

    // Required to make the menu dismiss when clicking outside
    SetForegroundWindow(m_hwnd);

    // Update menu state before showing
    UpdateMenuState();

    // Show menu
    UINT cmd = TrackPopupMenu(
        m_contextMenu,
        TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
        pt.x, pt.y,
        0,
        m_hwnd,
        nullptr);

    // Handle selection
    if (cmd != 0) {
        HandleMenuCommand(static_cast<MenuItem>(cmd));
    }

    // Required for menu to close properly
    PostMessageW(m_hwnd, WM_NULL, 0, 0);
}

void TrayIcon::HandleMenuCommand(MenuItem item) {
    if (!m_controller) return;

    switch (item) {
        case MenuItem::ToggleEnhancement:
            m_controller->EnableEnhancement(!m_controller->IsEnhancementEnabled());
            break;

        case MenuItem::ToggleMagnifier:
            m_controller->EnableMagnifier(!m_controller->IsMagnifierEnabled());
            break;

        case MenuItem::SpeakFocus:
            m_controller->SpeakFocusedElement();
            break;

        case MenuItem::Profile1:
            m_controller->SwitchProfile(L"profile1");
            break;

        case MenuItem::Profile2:
            m_controller->SwitchProfile(L"profile2");
            break;

        case MenuItem::Profile3:
            m_controller->SwitchProfile(L"profile3");
            break;

        case MenuItem::OpenSettings:
            // TODO: Open settings window
            ShowBalloon(L"Settings", L"Settings window not yet implemented");
            break;

        case MenuItem::PanicOff:
            m_controller->DisableAllEffects();
            break;

        case MenuItem::Exit:
            PostQuitMessage(0);
            break;
    }

    UpdateState();
}

std::wstring TrayIcon::GetTooltipText() {
    std::wstring text = L"Clarity Layer";

    if (m_controller) {
        if (m_controller->IsInSafeMode()) {
            text += L" (Safe Mode)";
        }
        else {
            text += L"\n";
            if (m_controller->IsEnhancementEnabled()) {
                text += L"✓ Enhancement ";
            }
            if (m_controller->IsMagnifierEnabled()) {
                text += L"✓ Magnifier";
            }
            if (!m_controller->IsEnhancementEnabled() && !m_controller->IsMagnifierEnabled()) {
                text += L"All effects off";
            }
        }
    }

    return text;
}

} // namespace clarity
