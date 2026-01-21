/**
 * Clarity Layer - Low-Level Mouse Hook
 * Detects mouse events for quick lens feature
 */

#pragma once

#include <Windows.h>
#include <functional>

namespace clarity {

/**
 * MouseHook provides global mouse event detection using a low-level hook.
 * Used for Ctrl+Right-Click quick lens feature.
 */
class MouseHook {
public:
    using Callback = std::function<void(UINT message, POINT pt, bool ctrlHeld)>;

    MouseHook() = default;
    ~MouseHook();

    // Non-copyable
    MouseHook(const MouseHook&) = delete;
    MouseHook& operator=(const MouseHook&) = delete;

    /**
     * Install the low-level mouse hook.
     * @param callback Function called on mouse events (WM_RBUTTONDOWN, WM_RBUTTONUP)
     * @return true if hook installed successfully
     */
    bool Install(Callback callback);

    /**
     * Uninstall the mouse hook.
     */
    void Uninstall();

    /**
     * Check if hook is currently installed.
     */
    bool IsInstalled() const { return m_hook != nullptr; }

private:
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);

    static MouseHook* s_instance;
    HHOOK m_hook = nullptr;
    Callback m_callback;
};

} // namespace clarity
