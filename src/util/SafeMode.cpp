/**
 * Clarity Layer - Safe Mode Implementation
 */

#include "SafeMode.h"
#include "Logger.h"
#include "AudioFeedback.h"
#include "core/Controller.h"

#include <Windows.h>
#include <atomic>

namespace clarity {

SafeMode::SafeMode() = default;

SafeMode::~SafeMode() {
    StopWatchdog();
}

void SafeMode::SetController(Controller* controller) {
    m_controller = controller;
}

bool SafeMode::CheckStartupSafeMode() {
    // Check if Shift key is held during startup
    SHORT shiftState = GetAsyncKeyState(VK_SHIFT);
    bool shiftHeld = (shiftState & 0x8000) != 0;

    if (shiftHeld) {
        LOG_INFO("Safe mode triggered: Shift key held during startup");
    }

    return shiftHeld;
}

void SafeMode::ActivatePanicOff() {
    LOG_INFO("PANIC OFF activated!");
    m_safeMode = true;

    // Call all registered panic callbacks
    for (const auto& callback : m_panicCallbacks) {
        try {
            callback();
        }
        catch (const std::exception& e) {
            LOG_ERROR("Panic callback threw exception: {}", e.what());
        }
        catch (...) {
            LOG_ERROR("Panic callback threw unknown exception");
        }
    }

    // Notify controller to disable everything
    if (m_controller) {
        m_controller->DisableAllEffects();
    }

    // Play panic-off confirmation sound
    // This is critical - user needs audio feedback that panic worked
    PlayPanicSound();

    LOG_INFO("Panic off complete - all effects disabled");
}

void SafeMode::ExitSafeMode() {
    if (!m_safeMode) return;

    LOG_INFO("Exiting safe mode");
    m_safeMode = false;

    // Don't automatically re-enable effects - let user do it manually
    // This is a safety feature
}

void SafeMode::RegisterPanicCallback(PanicCallback callback) {
    m_panicCallbacks.push_back(std::move(callback));
}

void SafeMode::StartWatchdog() {
    if (m_watchdogTimer) {
        StopWatchdog();
    }

    // Record initial heartbeat
    m_lastHeartbeat = GetTickCount();

    // Create timer queue timer
    HANDLE timerQueue = nullptr;
    if (!CreateTimerQueueTimer(
        reinterpret_cast<PHANDLE>(&m_watchdogTimer),
        timerQueue,
        WatchdogTimerCallback,
        this,
        WATCHDOG_TIMEOUT_MS,
        WATCHDOG_TIMEOUT_MS,
        WT_EXECUTEDEFAULT)) {
        LOG_ERROR("Failed to create watchdog timer");
        m_watchdogTimer = nullptr;
    }
    else {
        LOG_DEBUG("Watchdog timer started");
    }
}

void SafeMode::StopWatchdog() {
    if (m_watchdogTimer) {
        DeleteTimerQueueTimer(nullptr, m_watchdogTimer, INVALID_HANDLE_VALUE);
        m_watchdogTimer = nullptr;
        LOG_DEBUG("Watchdog timer stopped");
    }
}

void SafeMode::Heartbeat() {
    InterlockedExchange(&m_lastHeartbeat, GetTickCount());
}

void CALLBACK SafeMode::WatchdogTimerCallback(void* param, unsigned char timerOrWaitFired) {
    (void)timerOrWaitFired;

    auto* self = static_cast<SafeMode*>(param);
    if (!self) return;

    DWORD now = GetTickCount();
    DWORD lastBeat = static_cast<DWORD>(InterlockedCompareExchange(&self->m_lastHeartbeat, 0, 0));

    // Check if heartbeat is stale
    DWORD elapsed = now - lastBeat;
    if (elapsed > WATCHDOG_TIMEOUT_MS * 2) {
        LOG_WARN("Watchdog timeout! Last heartbeat was {}ms ago", elapsed);

        // Trigger panic off from watchdog
        // Note: This runs on timer thread, so we need to be careful
        self->ActivatePanicOff();
    }
}

// Note: PlayPanicSound is defined in AudioFeedback.cpp

} // namespace clarity
