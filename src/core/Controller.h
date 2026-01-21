/**
 * Clarity Layer - Core Controller
 * Central coordinator for all subsystems
 */

#pragma once

#include <Windows.h>
#include <memory>
#include <string>
#include <functional>

namespace clarity {

// Forward declarations
class ProfileManager;
class HotkeyService;
class CaptureManager;
class OverlayWindow;
class ShaderPipeline;
class MagnifierController;
class FocusTracker;
class AccessibilityReader;
class SpeechEngine;
class OcrReader;
class AudioFeedback;
class SafeMode;
class TrayIcon;

/**
 * Controller is the central coordinator that manages all subsystems.
 *
 * Responsibilities:
 * - Initialize and shut down all components
 * - Handle hotkey events and route to appropriate subsystems
 * - Manage application state and mode switching
 * - Handle system events (display change, DPI change, etc.)
 */
class Controller {
public:
    Controller(HWND hwnd, HINSTANCE hInstance);
    ~Controller();

    // Non-copyable
    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;

    // Initialize all subsystems
    bool Initialize(bool safeMode = false);

    // Shutdown all subsystems
    void Shutdown();

    // Save current state (for clean shutdown)
    void SaveState();

    // Handle hotkey event from message loop
    void HandleHotkey(int hotkeyId);

    // Handle custom window messages (e.g., tray icon)
    LRESULT HandleCustomMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // System event handlers
    void OnDisplayChange();
    void OnDpiChange(UINT dpi);
    void OnSystemResume();

    // Effect control
    void EnableEnhancement(bool enable);
    void EnableMagnifier(bool enable);
    void DisableAllEffects();  // Called by SafeMode

    // State queries
    bool IsEnhancementEnabled() const { return m_enhancementEnabled; }
    bool IsMagnifierEnabled() const { return m_magnifierEnabled; }
    bool IsInSafeMode() const;

    // Profile management
    void SwitchProfile(const std::wstring& profileName);
    void ReloadCurrentProfile();

    // Speech control
    void SpeakFocusedElement();
    void SpeakUnderCursor();
    void SpeakSelection();
    void StopSpeaking();

    // Magnifier control
    void ZoomIn();
    void ZoomOut();
    void SetZoomLevel(float level);
    void ToggleLensMode();
    void CycleFollowMode();

    // Get subsystems (for UI and other components)
    ProfileManager* GetProfileManager() { return m_profileManager.get(); }
    AudioFeedback* GetAudioFeedback() { return m_audioFeedback.get(); }
    SpeechEngine* GetSpeechEngine() { return m_speechEngine.get(); }

private:
    HWND m_hwnd;
    HINSTANCE m_hInstance;

    // Subsystems
    std::unique_ptr<ProfileManager> m_profileManager;
    std::unique_ptr<HotkeyService> m_hotkeyService;
    std::unique_ptr<CaptureManager> m_captureManager;
    std::unique_ptr<OverlayWindow> m_overlayWindow;
    std::unique_ptr<ShaderPipeline> m_shaderPipeline;
    std::unique_ptr<MagnifierController> m_magnifierController;
    std::unique_ptr<FocusTracker> m_focusTracker;
    std::unique_ptr<AccessibilityReader> m_accessibilityReader;
    std::unique_ptr<SpeechEngine> m_speechEngine;
    std::unique_ptr<OcrReader> m_ocrReader;
    std::unique_ptr<AudioFeedback> m_audioFeedback;
    std::unique_ptr<SafeMode> m_safeMode;
    std::unique_ptr<TrayIcon> m_trayIcon;

    // State
    bool m_enhancementEnabled = false;
    bool m_magnifierEnabled = false;
    bool m_initialized = false;
    bool m_startedInSafeMode = false;

    // Paths
    std::wstring m_appDataPath;
    std::wstring m_assetsPath;
    std::wstring m_profilesPath;

    // Helper methods
    bool InitializePaths();
    bool InitializeOverlay();
    bool InitializeHotkeys();
    bool InitializeReading();
    bool InitializeMagnifier();
    bool InitializeUI();

    void ApplyCurrentProfile();
    void UpdateOverlayEffects();
};

} // namespace clarity
