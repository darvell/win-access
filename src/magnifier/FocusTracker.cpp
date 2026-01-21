/**
 * Clarity Layer - Focus Tracker Implementation
 */

#include "FocusTracker.h"
#include "util/Logger.h"

#include <oleacc.h>

#pragma comment(lib, "oleacc.lib")

namespace clarity {

FocusTracker::FocusTracker() = default;

FocusTracker::~FocusTracker() {
    Stop();

    if (m_focusHandler) {
        if (m_automation) {
            m_automation->RemoveFocusChangedEventHandler(m_focusHandler);
        }
        m_focusHandler->Release();
        m_focusHandler = nullptr;
    }

    if (m_automation) {
        m_automation->Release();
        m_automation = nullptr;
    }
}

bool FocusTracker::Initialize() {
    // Create UI Automation
    HRESULT hr = CoCreateInstance(
        CLSID_CUIAutomation,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IUIAutomation,
        reinterpret_cast<void**>(&m_automation));

    if (FAILED(hr) || !m_automation) {
        LOG_ERROR("Failed to create UI Automation: 0x{:08X}", hr);
        return false;
    }

    // Create focus changed event handler
    m_focusHandler = new FocusChangedHandler(this);

    // Register for focus changed events
    hr = m_automation->AddFocusChangedEventHandler(nullptr, m_focusHandler);
    if (FAILED(hr)) {
        LOG_WARN("Failed to register focus changed handler: 0x{:08X}", hr);
        // Not fatal - we can still poll
    }

    // Get initial cursor position
    GetCursorPos(&m_lastCursorPos);

    LOG_INFO("FocusTracker initialized");
    return true;
}

void FocusTracker::SetFollowMode(FollowMode mode) {
    m_followMode = mode;
    LOG_DEBUG("Follow mode set to: {}",
              mode == FollowMode::Cursor ? "Cursor" :
              mode == FollowMode::Caret ? "Caret" : "Focus");
}

void FocusTracker::CycleFollowMode() {
    switch (m_followMode) {
        case FollowMode::Cursor:
            m_followMode = FollowMode::Caret;
            break;
        case FollowMode::Caret:
            m_followMode = FollowMode::KeyboardFocus;
            break;
        case FollowMode::KeyboardFocus:
            m_followMode = FollowMode::Cursor;
            break;
    }
}

POINT FocusTracker::GetFocusPoint() {
    switch (m_followMode) {
        case FollowMode::Cursor: {
            POINT pt;
            GetCursorPos(&pt);
            return pt;
        }
        case FollowMode::Caret:
            return GetCaretPosition();
        case FollowMode::KeyboardFocus:
            return GetFocusedElementCenter();
    }
    return { 0, 0 };
}

void FocusTracker::SetFocusChangeCallback(FocusChangeCallback callback) {
    m_callback = std::move(callback);
}

void FocusTracker::Start() {
    if (m_running) return;

    m_running = true;
    m_trackingThread = std::thread(&FocusTracker::TrackingLoop, this);

    LOG_DEBUG("Focus tracking started");
}

void FocusTracker::Stop() {
    if (!m_running) return;

    m_running = false;
    if (m_trackingThread.joinable()) {
        m_trackingThread.join();
    }

    LOG_DEBUG("Focus tracking stopped");
}

void FocusTracker::TrackingLoop() {
    while (m_running) {
        POINT newPos = GetFocusPoint();

        // Check if position changed
        bool changed = false;
        switch (m_followMode) {
            case FollowMode::Cursor:
                changed = (newPos.x != m_lastCursorPos.x || newPos.y != m_lastCursorPos.y);
                m_lastCursorPos = newPos;
                break;
            case FollowMode::Caret:
                changed = (newPos.x != m_lastCaretPos.x || newPos.y != m_lastCaretPos.y);
                m_lastCaretPos = newPos;
                break;
            case FollowMode::KeyboardFocus:
                changed = (newPos.x != m_lastFocusPos.x || newPos.y != m_lastFocusPos.y);
                m_lastFocusPos = newPos;
                break;
        }

        if (changed) {
            NotifyFocusChange(newPos);
        }

        // Sleep based on follow mode
        // Cursor needs faster updates
        int sleepMs = (m_followMode == FollowMode::Cursor) ? 16 : 50;
        Sleep(sleepMs);
    }
}

POINT FocusTracker::GetCaretPosition() {
    POINT caretPos = { 0, 0 };

    if (!m_automation) {
        return caretPos;
    }

    // Get focused element
    IUIAutomationElement* focused = nullptr;
    HRESULT hr = m_automation->GetFocusedElement(&focused);
    if (FAILED(hr) || !focused) {
        return caretPos;
    }

    // Try to get TextPattern for caret info
    IUIAutomationTextPattern2* textPattern = nullptr;
    hr = focused->GetCurrentPatternAs(UIA_TextPattern2Id,
                                       IID_IUIAutomationTextPattern2,
                                       reinterpret_cast<void**>(&textPattern));

    if (SUCCEEDED(hr) && textPattern) {
        IUIAutomationTextRange* caretRange = nullptr;
        hr = textPattern->GetCaretRange(nullptr, &caretRange);

        if (SUCCEEDED(hr) && caretRange) {
            SAFEARRAY* rects = nullptr;
            hr = caretRange->GetBoundingRectangles(&rects);

            if (SUCCEEDED(hr) && rects && rects->rgsabound[0].cElements >= 4) {
                double* rectData;
                SafeArrayAccessData(rects, reinterpret_cast<void**>(&rectData));

                // Use center of first bounding rectangle
                caretPos.x = static_cast<LONG>(rectData[0] + rectData[2] / 2);
                caretPos.y = static_cast<LONG>(rectData[1] + rectData[3] / 2);

                SafeArrayUnaccessData(rects);
            }

            if (rects) SafeArrayDestroy(rects);
            caretRange->Release();
        }

        textPattern->Release();
    }

    // Fallback: try to get caret from window
    if (caretPos.x == 0 && caretPos.y == 0) {
        HWND focusedWnd = GetForegroundWindow();
        DWORD threadId = GetWindowThreadProcessId(focusedWnd, nullptr);
        GUITHREADINFO guiInfo = { sizeof(GUITHREADINFO) };

        if (GetGUIThreadInfo(threadId, &guiInfo)) {
            if (guiInfo.hwndCaret) {
                caretPos.x = guiInfo.rcCaret.left;
                caretPos.y = guiInfo.rcCaret.top;
                ClientToScreen(guiInfo.hwndCaret, &caretPos);
            }
        }
    }

    focused->Release();
    return caretPos;
}

POINT FocusTracker::GetFocusedElementCenter() {
    POINT center = { 0, 0 };

    if (!m_automation) {
        return center;
    }

    IUIAutomationElement* focused = nullptr;
    HRESULT hr = m_automation->GetFocusedElement(&focused);
    if (FAILED(hr) || !focused) {
        return center;
    }

    RECT bounds;
    hr = focused->get_CurrentBoundingRectangle(&bounds);
    if (SUCCEEDED(hr)) {
        center.x = (bounds.left + bounds.right) / 2;
        center.y = (bounds.top + bounds.bottom) / 2;
    }

    focused->Release();
    return center;
}

void FocusTracker::NotifyFocusChange(POINT pt) {
    if (m_callback) {
        m_callback(pt);
    }
}

// FocusChangedHandler implementation
HRESULT STDMETHODCALLTYPE FocusChangedHandler::HandleFocusChangedEvent(IUIAutomationElement* sender) {
    if (!m_tracker) return S_OK;

    // Only handle if in keyboard focus mode
    if (m_tracker->GetFollowMode() != FollowMode::KeyboardFocus) {
        return S_OK;
    }

    RECT bounds;
    HRESULT hr = sender->get_CurrentBoundingRectangle(&bounds);
    if (SUCCEEDED(hr)) {
        POINT center;
        center.x = (bounds.left + bounds.right) / 2;
        center.y = (bounds.top + bounds.bottom) / 2;

        m_tracker->NotifyFocusChange(center);
    }

    return S_OK;
}

} // namespace clarity
