/**
 * Clarity Layer - Hotkey Service
 * Global hotkey registration and handling
 */

#pragma once

#include <Windows.h>
#include <map>
#include <string>
#include <functional>

namespace clarity {

/**
 * HotkeyService manages global hotkeys for the application.
 *
 * Hotkeys are registered with Windows and work even when the
 * application doesn't have focus.
 */
class HotkeyService {
public:
    // Predefined actions that can be bound to hotkeys
    enum class Action {
        None = 0,
        ToggleEnhancement,
        ToggleMagnifier,
        ZoomIn,
        ZoomOut,
        SpeakFocus,
        SpeakUnderCursor,
        SpeakSelection,
        StopSpeaking,
        PanicOff,
        SwitchProfile1,
        SwitchProfile2,
        SwitchProfile3,
        ToggleLensMode,
        CycleFollowMode,
        OpenSettings,

        // Keep this last
        ActionCount
    };

    explicit HotkeyService(HWND hwnd);
    ~HotkeyService();

    // Non-copyable
    HotkeyService(const HotkeyService&) = delete;
    HotkeyService& operator=(const HotkeyService&) = delete;

    // Register a hotkey for an action
    // modifiers: MOD_ALT, MOD_CONTROL, MOD_SHIFT, MOD_WIN
    // vk: Virtual key code
    bool RegisterHotkey(Action action, UINT modifiers, UINT vk);

    // Unregister a hotkey
    void UnregisterHotkey(Action action);

    // Unregister all hotkeys
    void UnregisterAll();

    // Get the action for a hotkey ID
    Action GetAction(int hotkeyId) const;

    // Check if an action has a hotkey registered
    bool IsRegistered(Action action) const;

    // Get hotkey description for UI
    static std::wstring GetHotkeyDescription(UINT modifiers, UINT vk);

    // Get action name for UI/logging
    static const char* GetActionName(Action action);

    // Parse hotkey string (e.g., "Ctrl+Alt+E") to modifiers and vk
    static bool ParseHotkeyString(const std::wstring& str, UINT& outModifiers, UINT& outVk);

    // Format hotkey to string
    static std::wstring FormatHotkey(UINT modifiers, UINT vk);

private:
    HWND m_hwnd;
    int m_nextId = 1;

    struct HotkeyInfo {
        int id;
        UINT modifiers;
        UINT vk;
    };

    std::map<Action, HotkeyInfo> m_hotkeys;
    std::map<int, Action> m_idToAction;
};

} // namespace clarity
