/**
 * Clarity Layer - Low-Level Mouse Hook Implementation
 */

#include "MouseHook.h"
#include "util/Logger.h"

namespace clarity {

// Static instance pointer for the hook callback
MouseHook* MouseHook::s_instance = nullptr;

MouseHook::~MouseHook() {
    Uninstall();
}

bool MouseHook::Install(Callback callback) {
    if (m_hook) {
        LOG_WARN("Mouse hook already installed");
        return true;
    }

    if (!callback) {
        LOG_ERROR("Cannot install mouse hook with null callback");
        return false;
    }

    m_callback = std::move(callback);
    s_instance = this;

    m_hook = SetWindowsHookExW(
        WH_MOUSE_LL,
        LowLevelMouseProc,
        GetModuleHandleW(nullptr),
        0
    );

    if (!m_hook) {
        LOG_ERROR("Failed to install mouse hook, error: {}", GetLastError());
        s_instance = nullptr;
        return false;
    }

    LOG_INFO("Mouse hook installed successfully");
    return true;
}

void MouseHook::Uninstall() {
    if (m_hook) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
        s_instance = nullptr;
        LOG_INFO("Mouse hook uninstalled");
    }
}

LRESULT CALLBACK MouseHook::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && s_instance && s_instance->m_callback) {
        auto* hookStruct = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        bool ctrlHeld = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;

        // Handle right button events
        if (wParam == WM_RBUTTONDOWN || wParam == WM_RBUTTONUP) {
            // Only invoke callback for Ctrl+RightClick down, or any right button up
            if (wParam == WM_RBUTTONUP || (wParam == WM_RBUTTONDOWN && ctrlHeld)) {
                s_instance->m_callback(
                    static_cast<UINT>(wParam),
                    hookStruct->pt,
                    ctrlHeld
                );
            }
        }
        // Handle mouse movement (for cursor tracking while quick lens is active)
        else if (wParam == WM_MOUSEMOVE) {
            s_instance->m_callback(
                static_cast<UINT>(wParam),
                hookStruct->pt,
                ctrlHeld
            );
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

} // namespace clarity
