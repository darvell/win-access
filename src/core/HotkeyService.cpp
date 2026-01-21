/**
 * Clarity Layer - Hotkey Service Implementation
 */

#include "HotkeyService.h"
#include "util/Logger.h"

#include <sstream>
#include <algorithm>
#include <cctype>

namespace clarity {

HotkeyService::HotkeyService(HWND hwnd)
    : m_hwnd(hwnd) {
}

HotkeyService::~HotkeyService() {
    UnregisterAll();
}

bool HotkeyService::RegisterHotkey(Action action, UINT modifiers, UINT vk) {
    // Check if action already has a hotkey
    if (m_hotkeys.find(action) != m_hotkeys.end()) {
        UnregisterHotkey(action);
    }

    int id = m_nextId++;

    // Try to register the hotkey with Windows
    if (!::RegisterHotKey(m_hwnd, id, modifiers, vk)) {
        DWORD error = GetLastError();
        LOG_ERROR("Failed to register hotkey {} (modifiers={}, vk={}): error {}",
                  GetActionName(action), modifiers, vk, error);

        if (error == ERROR_HOTKEY_ALREADY_REGISTERED) {
            LOG_WARN("Hotkey is already registered by another application");
        }
        return false;
    }

    HotkeyInfo info = { id, modifiers, vk };
    m_hotkeys[action] = info;
    m_idToAction[id] = action;

    LOG_INFO("Registered hotkey: {} -> {}", GetActionName(action),
             std::string(FormatHotkey(modifiers, vk).begin(), FormatHotkey(modifiers, vk).end()));

    return true;
}

void HotkeyService::UnregisterHotkey(Action action) {
    auto it = m_hotkeys.find(action);
    if (it == m_hotkeys.end()) {
        return;
    }

    ::UnregisterHotKey(m_hwnd, it->second.id);
    m_idToAction.erase(it->second.id);
    m_hotkeys.erase(it);

    LOG_DEBUG("Unregistered hotkey for action: {}", GetActionName(action));
}

void HotkeyService::UnregisterAll() {
    for (const auto& [action, info] : m_hotkeys) {
        ::UnregisterHotKey(m_hwnd, info.id);
    }
    m_hotkeys.clear();
    m_idToAction.clear();

    LOG_DEBUG("Unregistered all hotkeys");
}

HotkeyService::Action HotkeyService::GetAction(int hotkeyId) const {
    auto it = m_idToAction.find(hotkeyId);
    if (it != m_idToAction.end()) {
        return it->second;
    }
    return Action::None;
}

bool HotkeyService::IsRegistered(Action action) const {
    return m_hotkeys.find(action) != m_hotkeys.end();
}

std::wstring HotkeyService::GetHotkeyDescription(UINT modifiers, UINT vk) {
    return FormatHotkey(modifiers, vk);
}

const char* HotkeyService::GetActionName(Action action) {
    switch (action) {
        case Action::None: return "None";
        case Action::ToggleEnhancement: return "ToggleEnhancement";
        case Action::ToggleMagnifier: return "ToggleMagnifier";
        case Action::ZoomIn: return "ZoomIn";
        case Action::ZoomOut: return "ZoomOut";
        case Action::SpeakFocus: return "SpeakFocus";
        case Action::SpeakUnderCursor: return "SpeakUnderCursor";
        case Action::SpeakSelection: return "SpeakSelection";
        case Action::StopSpeaking: return "StopSpeaking";
        case Action::PanicOff: return "PanicOff";
        case Action::SwitchProfile1: return "SwitchProfile1";
        case Action::SwitchProfile2: return "SwitchProfile2";
        case Action::SwitchProfile3: return "SwitchProfile3";
        case Action::ToggleLensMode: return "ToggleLensMode";
        case Action::CycleFollowMode: return "CycleFollowMode";
        case Action::OpenSettings: return "OpenSettings";
        default: return "Unknown";
    }
}

bool HotkeyService::ParseHotkeyString(const std::wstring& str, UINT& outModifiers, UINT& outVk) {
    outModifiers = 0;
    outVk = 0;

    // Split by + character
    std::wstring remaining = str;
    std::vector<std::wstring> parts;

    size_t pos;
    while ((pos = remaining.find(L'+')) != std::wstring::npos) {
        std::wstring part = remaining.substr(0, pos);
        // Trim whitespace
        while (!part.empty() && std::iswspace(part.front())) part.erase(0, 1);
        while (!part.empty() && std::iswspace(part.back())) part.pop_back();
        if (!part.empty()) {
            parts.push_back(part);
        }
        remaining = remaining.substr(pos + 1);
    }
    // Add final part
    while (!remaining.empty() && std::iswspace(remaining.front())) remaining.erase(0, 1);
    while (!remaining.empty() && std::iswspace(remaining.back())) remaining.pop_back();
    if (!remaining.empty()) {
        parts.push_back(remaining);
    }

    if (parts.empty()) {
        return false;
    }

    // Last part is the key, rest are modifiers
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        std::wstring mod = parts[i];
        // Convert to uppercase for comparison
        std::transform(mod.begin(), mod.end(), mod.begin(), ::towupper);

        if (mod == L"CTRL" || mod == L"CONTROL") {
            outModifiers |= MOD_CONTROL;
        }
        else if (mod == L"ALT") {
            outModifiers |= MOD_ALT;
        }
        else if (mod == L"SHIFT") {
            outModifiers |= MOD_SHIFT;
        }
        else if (mod == L"WIN" || mod == L"WINDOWS" || mod == L"META") {
            outModifiers |= MOD_WIN;
        }
        else {
            LOG_WARN("Unknown modifier: {}", std::string(mod.begin(), mod.end()));
            return false;
        }
    }

    // Parse the key
    std::wstring key = parts.back();
    std::transform(key.begin(), key.end(), key.begin(), ::towupper);

    // Single character
    if (key.length() == 1) {
        wchar_t ch = key[0];
        if (ch >= L'A' && ch <= L'Z') {
            outVk = ch;
            return true;
        }
        if (ch >= L'0' && ch <= L'9') {
            outVk = ch;
            return true;
        }
    }

    // Special keys
    if (key == L"ESCAPE" || key == L"ESC") { outVk = VK_ESCAPE; return true; }
    if (key == L"SPACE") { outVk = VK_SPACE; return true; }
    if (key == L"ENTER" || key == L"RETURN") { outVk = VK_RETURN; return true; }
    if (key == L"TAB") { outVk = VK_TAB; return true; }
    if (key == L"BACKSPACE") { outVk = VK_BACK; return true; }
    if (key == L"DELETE" || key == L"DEL") { outVk = VK_DELETE; return true; }
    if (key == L"INSERT" || key == L"INS") { outVk = VK_INSERT; return true; }
    if (key == L"HOME") { outVk = VK_HOME; return true; }
    if (key == L"END") { outVk = VK_END; return true; }
    if (key == L"PAGEUP" || key == L"PGUP") { outVk = VK_PRIOR; return true; }
    if (key == L"PAGEDOWN" || key == L"PGDN") { outVk = VK_NEXT; return true; }
    if (key == L"UP") { outVk = VK_UP; return true; }
    if (key == L"DOWN") { outVk = VK_DOWN; return true; }
    if (key == L"LEFT") { outVk = VK_LEFT; return true; }
    if (key == L"RIGHT") { outVk = VK_RIGHT; return true; }
    if (key == L"PLUS" || key == L"=") { outVk = VK_OEM_PLUS; return true; }
    if (key == L"MINUS" || key == L"-") { outVk = VK_OEM_MINUS; return true; }

    // Function keys
    if (key.length() >= 2 && key[0] == L'F') {
        int fNum = _wtoi(key.c_str() + 1);
        if (fNum >= 1 && fNum <= 24) {
            outVk = VK_F1 + (fNum - 1);
            return true;
        }
    }

    LOG_WARN("Unknown key: {}", std::string(key.begin(), key.end()));
    return false;
}

std::wstring HotkeyService::FormatHotkey(UINT modifiers, UINT vk) {
    std::wstringstream ss;

    if (modifiers & MOD_CONTROL) ss << L"Ctrl+";
    if (modifiers & MOD_ALT) ss << L"Alt+";
    if (modifiers & MOD_SHIFT) ss << L"Shift+";
    if (modifiers & MOD_WIN) ss << L"Win+";

    // Format key name
    if (vk >= 'A' && vk <= 'Z') {
        ss << static_cast<wchar_t>(vk);
    }
    else if (vk >= '0' && vk <= '9') {
        ss << static_cast<wchar_t>(vk);
    }
    else if (vk >= VK_F1 && vk <= VK_F24) {
        ss << L"F" << (vk - VK_F1 + 1);
    }
    else {
        switch (vk) {
            case VK_ESCAPE: ss << L"Esc"; break;
            case VK_SPACE: ss << L"Space"; break;
            case VK_RETURN: ss << L"Enter"; break;
            case VK_TAB: ss << L"Tab"; break;
            case VK_BACK: ss << L"Backspace"; break;
            case VK_DELETE: ss << L"Delete"; break;
            case VK_INSERT: ss << L"Insert"; break;
            case VK_HOME: ss << L"Home"; break;
            case VK_END: ss << L"End"; break;
            case VK_PRIOR: ss << L"PageUp"; break;
            case VK_NEXT: ss << L"PageDown"; break;
            case VK_UP: ss << L"Up"; break;
            case VK_DOWN: ss << L"Down"; break;
            case VK_LEFT: ss << L"Left"; break;
            case VK_RIGHT: ss << L"Right"; break;
            case VK_OEM_PLUS: ss << L"Plus"; break;
            case VK_OEM_MINUS: ss << L"Minus"; break;
            default: ss << L"0x" << std::hex << vk; break;
        }
    }

    return ss.str();
}

} // namespace clarity
