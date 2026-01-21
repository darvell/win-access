/**
 * Clarity Layer - Safe Mode
 * Panic-off functionality and recovery mechanisms
 */

#pragma once

#include <Windows.h>
#include <functional>
#include <vector>

namespace clarity {

// Forward declarations
class Controller;

/**
 * SafeMode handles emergency shutdown and recovery scenarios.
 *
 * Features:
 * - Panic-off: Instantly disables all visual effects
 * - Startup safe mode: Hold Shift during launch to start with effects disabled
 * - Watchdog: Auto-recovery if app becomes unresponsive
 */
class SafeMode {
public:
    SafeMode();
    ~SafeMode();

    // Set the controller reference for callbacks
    void SetController(Controller* controller);

    // Check if Shift is held during startup (safe mode trigger)
    static bool CheckStartupSafeMode();

    // Activate panic-off - immediately disable all effects
    void ActivatePanicOff();

    // Check if currently in safe mode
    bool IsInSafeMode() const { return m_safeMode; }

    // Exit safe mode and resume normal operation
    void ExitSafeMode();

    // Register a callback to be called when panic-off is triggered
    using PanicCallback = std::function<void()>;
    void RegisterPanicCallback(PanicCallback callback);

    // Start watchdog timer
    void StartWatchdog();

    // Stop watchdog timer
    void StopWatchdog();

    // Called periodically to indicate the app is responsive
    void Heartbeat();

private:
    bool m_safeMode = false;
    Controller* m_controller = nullptr;
    std::vector<PanicCallback> m_panicCallbacks;

    // Watchdog state
    void* m_watchdogTimer = nullptr;  // HANDLE
    volatile long m_lastHeartbeat = 0;
    static constexpr int WATCHDOG_TIMEOUT_MS = 5000;

    static void CALLBACK WatchdogTimerCallback(void* param, unsigned char timerOrWaitFired);
};

} // namespace clarity
