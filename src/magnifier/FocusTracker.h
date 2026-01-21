/**
 * Clarity Layer - Focus Tracker
 * Tracks cursor, caret, and keyboard focus positions
 */

#pragma once

#include <Windows.h>
#include <UIAutomation.h>
#include <functional>
#include <atomic>
#include <thread>

#include "core/ProfileManager.h"  // For FollowMode

namespace clarity {

/**
 * FocusTracker monitors the current point of interest based on mode:
 * - Cursor: Follows mouse cursor
 * - Caret: Follows text caret in focused window
 * - KeyboardFocus: Follows currently focused UI element
 */
class FocusTracker {
public:
    FocusTracker();
    ~FocusTracker();

    // Non-copyable
    FocusTracker(const FocusTracker&) = delete;
    FocusTracker& operator=(const FocusTracker&) = delete;

    // Initialize with UI Automation
    bool Initialize();

    // Set follow mode
    void SetFollowMode(FollowMode mode);
    FollowMode GetFollowMode() const { return m_followMode; }

    // Cycle through follow modes
    void CycleFollowMode();

    // Get current focus point
    POINT GetFocusPoint();

    // Register callback for focus changes
    using FocusChangeCallback = std::function<void(POINT)>;
    void SetFocusChangeCallback(FocusChangeCallback callback);

    // Start/stop tracking
    void Start();
    void Stop();
    bool IsRunning() const { return m_running; }

private:
    FollowMode m_followMode = FollowMode::Cursor;
    FocusChangeCallback m_callback;

    // UI Automation
    IUIAutomation* m_automation = nullptr;
    IUIAutomationFocusChangedEventHandler* m_focusHandler = nullptr;

    // Tracking thread
    std::atomic<bool> m_running{false};
    std::thread m_trackingThread;

    // Last known positions
    POINT m_lastCursorPos = { 0, 0 };
    POINT m_lastCaretPos = { 0, 0 };
    POINT m_lastFocusPos = { 0, 0 };

    // Tracking loop
    void TrackingLoop();

    // Get caret position using UI Automation
    POINT GetCaretPosition();

    // Get focused element center
    POINT GetFocusedElementCenter();

    // Notify callback of position change
    void NotifyFocusChange(POINT pt);
};

/**
 * UI Automation focus changed event handler
 */
class FocusChangedHandler : public IUIAutomationFocusChangedEventHandler {
public:
    explicit FocusChangedHandler(FocusTracker* tracker) : m_tracker(tracker), m_refCount(1) {}

    // IUnknown
    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&m_refCount);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = InterlockedDecrement(&m_refCount);
        if (count == 0) {
            delete this;
        }
        return count;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
        if (riid == IID_IUnknown || riid == IID_IUIAutomationFocusChangedEventHandler) {
            *ppvObject = this;
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    // IUIAutomationFocusChangedEventHandler
    HRESULT STDMETHODCALLTYPE HandleFocusChangedEvent(IUIAutomationElement* sender) override;

private:
    FocusTracker* m_tracker;
    LONG m_refCount;
};

} // namespace clarity
