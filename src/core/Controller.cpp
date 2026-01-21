/**
 * Clarity Layer - Core Controller Implementation
 */

#include "Controller.h"
#include "ProfileManager.h"
#include "HotkeyService.h"
#include "overlay/CaptureManager.h"
#include "overlay/OverlayWindow.h"
#include "overlay/ShaderPipeline.h"
#include "magnifier/MagnifierController.h"
#include "magnifier/FocusTracker.h"
#include "reader/AccessibilityReader.h"
#include "reader/SpeechEngine.h"
#include "reader/OcrReader.h"
#include "ui/TrayIcon.h"
#include "util/Logger.h"
#include "util/AudioFeedback.h"
#include "util/SafeMode.h"

#include <ShlObj.h>
#include <filesystem>

namespace clarity {

Controller::Controller(HWND hwnd, HINSTANCE hInstance)
    : m_hwnd(hwnd)
    , m_hInstance(hInstance) {
}

Controller::~Controller() {
    if (m_initialized) {
        Shutdown();
    }
}

bool Controller::Initialize(bool safeMode) {
    LOG_INFO("Initializing Controller (safeMode={})", safeMode);
    m_startedInSafeMode = safeMode;

    // Initialize paths first
    if (!InitializePaths()) {
        LOG_ERROR("Failed to initialize paths");
        return false;
    }

    // Create safe mode handler first (needed for panic-off)
    m_safeMode = std::make_unique<SafeMode>();
    m_safeMode->SetController(this);

    if (safeMode) {
        m_safeMode->ActivatePanicOff();
    }

    // Initialize audio feedback (for user confirmation sounds)
    m_audioFeedback = std::make_unique<AudioFeedback>();
    if (!m_audioFeedback->Initialize(m_assetsPath)) {
        LOG_WARN("Audio feedback initialization failed - continuing without sounds");
    }

    // Register panic callback with audio
    m_safeMode->RegisterPanicCallback([this]() {
        m_audioFeedback->Play(Sound::PanicOff);
    });

    // Initialize profile manager
    m_profileManager = std::make_unique<ProfileManager>();
    if (!m_profileManager->Initialize(m_profilesPath)) {
        LOG_ERROR("Failed to initialize profile manager");
        return false;
    }

    // Load default profile
    if (!m_profileManager->LoadProfile(L"default")) {
        LOG_WARN("Default profile not found, creating default settings");
        m_profileManager->CreateDefaultProfile();
    }

    // Initialize hotkey service (must be before overlay so panic-off works)
    if (!InitializeHotkeys()) {
        LOG_ERROR("Failed to initialize hotkeys");
        return false;
    }

    // Initialize overlay (skip if in safe mode)
    if (!safeMode) {
        if (!InitializeOverlay()) {
            LOG_ERROR("Failed to initialize overlay");
            // Not fatal - user can still use other features
        }
    }

    // Initialize reading features
    if (!InitializeReading()) {
        LOG_WARN("Reading features initialization failed");
        // Not fatal
    }

    // Initialize magnifier
    if (!safeMode) {
        if (!InitializeMagnifier()) {
            LOG_WARN("Magnifier initialization failed");
            // Not fatal
        }
    }

    // Initialize UI (tray icon, etc.)
    if (!InitializeUI()) {
        LOG_ERROR("Failed to initialize UI");
        return false;
    }

    // Apply current profile settings
    ApplyCurrentProfile();

    // Start watchdog
    m_safeMode->StartWatchdog();

    m_initialized = true;
    LOG_INFO("Controller initialization complete");

    // Play startup sound
    m_audioFeedback->Play(Sound::Enable);

    return true;
}

void Controller::Shutdown() {
    LOG_INFO("Shutting down Controller");

    // Stop watchdog
    if (m_safeMode) {
        m_safeMode->StopWatchdog();
    }

    // Disable all effects first
    DisableAllEffects();

    // Save state
    SaveState();

    // Shutdown in reverse order
    m_trayIcon.reset();
    m_ocrReader.reset();
    m_speechEngine.reset();
    m_accessibilityReader.reset();
    m_focusTracker.reset();
    m_magnifierController.reset();
    m_shaderPipeline.reset();
    m_overlayWindow.reset();
    m_captureManager.reset();
    m_hotkeyService.reset();
    m_profileManager.reset();
    m_audioFeedback.reset();
    m_safeMode.reset();

    m_initialized = false;
    LOG_INFO("Controller shutdown complete");
}

void Controller::SaveState() {
    if (m_profileManager) {
        m_profileManager->SaveCurrentProfile();
    }
}

bool Controller::InitializePaths() {
    // Get AppData path
    wchar_t* appDataPath = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appDataPath))) {
        LOG_ERROR("Failed to get AppData path");
        return false;
    }

    m_appDataPath = std::filesystem::path(appDataPath) / L"ClarityLayer";
    CoTaskMemFree(appDataPath);

    // Create app data directory
    std::filesystem::create_directories(m_appDataPath);

    // Get executable directory for assets
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();

    m_assetsPath = (exeDir / L"assets").wstring();
    m_profilesPath = (exeDir / L"profiles").wstring();

    // Also check user profiles directory in AppData
    std::filesystem::path userProfiles = std::filesystem::path(m_appDataPath) / L"profiles";
    std::filesystem::create_directories(userProfiles);

    LOG_INFO("Paths initialized:");
    LOG_INFO("  AppData: {}", std::filesystem::path(m_appDataPath).string());
    LOG_INFO("  Assets:  {}", std::filesystem::path(m_assetsPath).string());
    LOG_INFO("  Profiles: {}", std::filesystem::path(m_profilesPath).string());

    return true;
}

bool Controller::InitializeOverlay() {
    LOG_INFO("Initializing overlay subsystem");

    // Create D3D11 overlay window
    m_overlayWindow = std::make_unique<OverlayWindow>();
    if (!m_overlayWindow->Initialize(m_hInstance)) {
        LOG_ERROR("Failed to create overlay window");
        return false;
    }

    // Create capture manager
    m_captureManager = std::make_unique<CaptureManager>();
    if (!m_captureManager->Initialize(m_overlayWindow->GetD3DDevice())) {
        LOG_ERROR("Failed to initialize capture manager");
        return false;
    }

    // Create shader pipeline
    m_shaderPipeline = std::make_unique<ShaderPipeline>();
    std::wstring shadersPath = std::filesystem::path(m_assetsPath).parent_path() / L"shaders";
    if (!m_shaderPipeline->Initialize(m_overlayWindow->GetD3DDevice(), shadersPath)) {
        LOG_ERROR("Failed to initialize shader pipeline");
        return false;
    }

    // Connect capture to overlay rendering
    m_captureManager->SetFrameCallback([this](ID3D11Texture2D* frame) {
        if (m_enhancementEnabled && m_overlayWindow && m_shaderPipeline) {
            // Apply shader transforms and render
            auto transformedFrame = m_shaderPipeline->Process(frame);
            m_overlayWindow->RenderFrame(transformedFrame);
        }

        // Heartbeat for watchdog
        if (m_safeMode) {
            m_safeMode->Heartbeat();
        }
    });

    LOG_INFO("Overlay subsystem initialized");
    return true;
}

bool Controller::InitializeHotkeys() {
    LOG_INFO("Initializing hotkey service");

    m_hotkeyService = std::make_unique<HotkeyService>(m_hwnd);

    // Register default hotkeys
    // These can be customized via profile
    using Action = HotkeyService::Action;

    // Panic off - ALWAYS registered, cannot be changed
    m_hotkeyService->RegisterHotkey(Action::PanicOff, MOD_CONTROL | MOD_ALT, 'X');

    // Core hotkeys
    m_hotkeyService->RegisterHotkey(Action::ToggleEnhancement, MOD_WIN, 'E');
    m_hotkeyService->RegisterHotkey(Action::ToggleMagnifier, MOD_WIN, 'M');
    m_hotkeyService->RegisterHotkey(Action::ZoomIn, MOD_WIN, VK_OEM_PLUS);
    m_hotkeyService->RegisterHotkey(Action::ZoomOut, MOD_WIN, VK_OEM_MINUS);

    // Speech hotkeys
    m_hotkeyService->RegisterHotkey(Action::SpeakFocus, MOD_WIN, 'F');
    m_hotkeyService->RegisterHotkey(Action::SpeakUnderCursor, MOD_WIN, 'S');
    m_hotkeyService->RegisterHotkey(Action::StopSpeaking, 0, VK_ESCAPE);

    // Profile hotkeys
    m_hotkeyService->RegisterHotkey(Action::SwitchProfile1, MOD_WIN, '1');
    m_hotkeyService->RegisterHotkey(Action::SwitchProfile2, MOD_WIN, '2');
    m_hotkeyService->RegisterHotkey(Action::SwitchProfile3, MOD_WIN, '3');

    // Magnifier modes
    m_hotkeyService->RegisterHotkey(Action::ToggleLensMode, MOD_WIN, 'L');
    m_hotkeyService->RegisterHotkey(Action::CycleFollowMode, MOD_WIN, 'T');

    LOG_INFO("Hotkey service initialized");
    return true;
}

bool Controller::InitializeReading() {
    LOG_INFO("Initializing reading subsystem");

    // UI Automation reader
    m_accessibilityReader = std::make_unique<AccessibilityReader>();
    if (!m_accessibilityReader->Initialize()) {
        LOG_WARN("UI Automation initialization failed");
        // Continue - we can still use OCR
    }

    // Speech engine
    m_speechEngine = std::make_unique<SpeechEngine>();
    if (!m_speechEngine->Initialize()) {
        LOG_WARN("Speech engine initialization failed");
        // Not fatal but reduces functionality
    }

    // OCR reader (fallback for non-accessible content)
    m_ocrReader = std::make_unique<OcrReader>();
    if (!m_ocrReader->Initialize()) {
        LOG_WARN("OCR initialization failed");
        // Not fatal
    }

    LOG_INFO("Reading subsystem initialized");
    return true;
}

bool Controller::InitializeMagnifier() {
    LOG_INFO("Initializing magnifier subsystem");

    m_focusTracker = std::make_unique<FocusTracker>();
    if (!m_focusTracker->Initialize()) {
        LOG_WARN("Focus tracker initialization failed");
    }

    m_magnifierController = std::make_unique<MagnifierController>();
    if (!m_magnifierController->Initialize()) {
        LOG_WARN("Magnifier controller initialization failed");
        return false;
    }

    // Connect focus tracker to magnifier
    m_focusTracker->SetFocusChangeCallback([this](POINT pt) {
        if (m_magnifierEnabled && m_magnifierController) {
            m_magnifierController->SetFocusPoint(pt);
        }
    });

    LOG_INFO("Magnifier subsystem initialized");
    return true;
}

bool Controller::InitializeUI() {
    LOG_INFO("Initializing UI");

    m_trayIcon = std::make_unique<TrayIcon>();
    if (!m_trayIcon->Initialize(m_hwnd, m_hInstance, this)) {
        LOG_ERROR("Failed to initialize tray icon");
        return false;
    }

    LOG_INFO("UI initialized");
    return true;
}

void Controller::HandleHotkey(int hotkeyId) {
    using Action = HotkeyService::Action;

    Action action = m_hotkeyService->GetAction(hotkeyId);
    LOG_DEBUG("Hotkey pressed: action={}", static_cast<int>(action));

    switch (action) {
        case Action::PanicOff:
            m_safeMode->ActivatePanicOff();
            break;

        case Action::ToggleEnhancement:
            EnableEnhancement(!m_enhancementEnabled);
            break;

        case Action::ToggleMagnifier:
            EnableMagnifier(!m_magnifierEnabled);
            break;

        case Action::ZoomIn:
            ZoomIn();
            break;

        case Action::ZoomOut:
            ZoomOut();
            break;

        case Action::SpeakFocus:
            SpeakFocusedElement();
            break;

        case Action::SpeakUnderCursor:
            SpeakUnderCursor();
            break;

        case Action::StopSpeaking:
            StopSpeaking();
            break;

        case Action::SwitchProfile1:
            SwitchProfile(L"profile1");
            break;

        case Action::SwitchProfile2:
            SwitchProfile(L"profile2");
            break;

        case Action::SwitchProfile3:
            SwitchProfile(L"profile3");
            break;

        case Action::ToggleLensMode:
            ToggleLensMode();
            break;

        case Action::CycleFollowMode:
            CycleFollowMode();
            break;

        default:
            LOG_WARN("Unknown hotkey action: {}", static_cast<int>(action));
            break;
    }
}

LRESULT Controller::HandleCustomMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    // Handle tray icon messages
    if (m_trayIcon) {
        return m_trayIcon->HandleMessage(msg, wParam, lParam);
    }
    return 0;
}

void Controller::OnDisplayChange() {
    LOG_INFO("Display configuration changed");

    if (m_captureManager) {
        m_captureManager->OnDisplayChange();
    }

    if (m_overlayWindow) {
        m_overlayWindow->OnDisplayChange();
    }

    if (m_magnifierController) {
        m_magnifierController->OnDisplayChange();
    }
}

void Controller::OnDpiChange(UINT dpi) {
    LOG_INFO("DPI changed to {}", dpi);

    if (m_overlayWindow) {
        m_overlayWindow->OnDpiChange(dpi);
    }
}

void Controller::OnSystemResume() {
    LOG_INFO("System resumed from sleep");

    // Reinitialize capture sessions which may have been invalidated
    if (m_captureManager && m_enhancementEnabled) {
        m_captureManager->Restart();
    }
}

void Controller::EnableEnhancement(bool enable) {
    if (m_enhancementEnabled == enable) return;
    if (m_safeMode && m_safeMode->IsInSafeMode() && enable) {
        LOG_WARN("Cannot enable enhancement in safe mode");
        return;
    }

    m_enhancementEnabled = enable;
    LOG_INFO("Enhancement {}", enable ? "enabled" : "disabled");

    if (enable) {
        if (m_captureManager) {
            m_captureManager->Start();
        }
        if (m_overlayWindow) {
            m_overlayWindow->Show();
        }
        m_audioFeedback->Play(Sound::Enable);
    }
    else {
        if (m_captureManager) {
            m_captureManager->Stop();
        }
        if (m_overlayWindow) {
            m_overlayWindow->Hide();
        }
        m_audioFeedback->Play(Sound::Disable);
    }

    if (m_trayIcon) {
        m_trayIcon->UpdateState();
    }
}

void Controller::EnableMagnifier(bool enable) {
    if (m_magnifierEnabled == enable) return;
    if (m_safeMode && m_safeMode->IsInSafeMode() && enable) {
        LOG_WARN("Cannot enable magnifier in safe mode");
        return;
    }

    m_magnifierEnabled = enable;
    LOG_INFO("Magnifier {}", enable ? "enabled" : "disabled");

    if (m_magnifierController) {
        if (enable) {
            m_magnifierController->Enable();
            m_audioFeedback->Play(Sound::Enable);
        }
        else {
            m_magnifierController->Disable();
            m_audioFeedback->Play(Sound::Disable);
        }
    }

    if (m_trayIcon) {
        m_trayIcon->UpdateState();
    }
}

void Controller::DisableAllEffects() {
    LOG_INFO("Disabling all effects");

    // Stop enhancement
    m_enhancementEnabled = false;
    if (m_captureManager) {
        m_captureManager->Stop();
    }
    if (m_overlayWindow) {
        m_overlayWindow->Hide();
    }

    // Stop magnifier
    m_magnifierEnabled = false;
    if (m_magnifierController) {
        m_magnifierController->Disable();
    }

    // Stop speech
    if (m_speechEngine) {
        m_speechEngine->Stop();
    }

    if (m_trayIcon) {
        m_trayIcon->UpdateState();
    }
}

bool Controller::IsInSafeMode() const {
    return m_safeMode && m_safeMode->IsInSafeMode();
}

void Controller::SwitchProfile(const std::wstring& profileName) {
    if (!m_profileManager) return;

    LOG_INFO("Switching to profile: {}", std::filesystem::path(profileName).string());

    if (m_profileManager->LoadProfile(profileName)) {
        ApplyCurrentProfile();
        m_audioFeedback->Play(Sound::ProfileSwitch);
    }
    else {
        LOG_WARN("Failed to load profile: {}", std::filesystem::path(profileName).string());
        m_audioFeedback->Play(Sound::Error);
    }
}

void Controller::ReloadCurrentProfile() {
    ApplyCurrentProfile();
}

void Controller::ApplyCurrentProfile() {
    if (!m_profileManager) return;

    const auto& profile = m_profileManager->GetCurrentProfile();
    LOG_INFO("Applying profile: {}", std::filesystem::path(profile.name).string());

    // Apply visual settings
    if (m_shaderPipeline) {
        m_shaderPipeline->SetContrast(profile.visual.contrast);
        m_shaderPipeline->SetBrightness(profile.visual.brightness);
        m_shaderPipeline->SetGamma(profile.visual.gamma);
        m_shaderPipeline->SetSaturation(profile.visual.saturation);
        m_shaderPipeline->SetInvertMode(profile.visual.invertMode);
        m_shaderPipeline->SetEdgeStrength(profile.visual.edgeStrength);
    }

    // Apply magnifier settings
    if (m_magnifierController) {
        m_magnifierController->SetZoomLevel(profile.magnifier.zoomLevel);
        m_magnifierController->SetLensMode(profile.magnifier.lensMode);
        m_magnifierController->SetLensSize(profile.magnifier.lensSize);
    }

    if (m_focusTracker) {
        m_focusTracker->SetFollowMode(profile.magnifier.followMode);
    }

    // Apply speech settings
    if (m_speechEngine) {
        m_speechEngine->SetRate(profile.speech.rate);
        m_speechEngine->SetVolume(profile.speech.volume);
    }

    // Enable/disable based on profile
    if (!IsInSafeMode()) {
        EnableEnhancement(profile.visual.enabled);
        EnableMagnifier(profile.magnifier.enabled);
    }
}

void Controller::UpdateOverlayEffects() {
    // Force shader recompilation or parameter update
    if (m_shaderPipeline) {
        m_shaderPipeline->UpdateParameters();
    }
}

void Controller::SpeakFocusedElement() {
    if (!m_accessibilityReader || !m_speechEngine) return;

    m_audioFeedback->Play(Sound::SpeakStart);

    std::wstring text = m_accessibilityReader->GetFocusedElementText();
    if (!text.empty()) {
        m_speechEngine->Speak(text);
    }
    else {
        // Fallback to OCR if no accessible text
        if (m_ocrReader) {
            RECT focusRect = m_accessibilityReader->GetFocusedElementBounds();
            m_ocrReader->RecognizeRegion(focusRect, [this](const std::wstring& ocrText) {
                if (!ocrText.empty()) {
                    m_speechEngine->Speak(ocrText);
                }
                else {
                    m_speechEngine->Speak(L"No text found");
                }
            });
        }
        else {
            m_speechEngine->Speak(L"No text available");
        }
    }
}

void Controller::SpeakUnderCursor() {
    if (!m_speechEngine) return;

    m_audioFeedback->Play(Sound::SpeakStart);

    POINT cursorPos;
    GetCursorPos(&cursorPos);

    // Try UI Automation first
    if (m_accessibilityReader) {
        std::wstring text = m_accessibilityReader->GetElementTextAtPoint(cursorPos);
        if (!text.empty()) {
            m_speechEngine->Speak(text);
            return;
        }
    }

    // Fallback to OCR
    if (m_ocrReader) {
        // Create a region around the cursor
        RECT region = {
            cursorPos.x - 100,
            cursorPos.y - 50,
            cursorPos.x + 100,
            cursorPos.y + 50
        };
        m_ocrReader->RecognizeRegion(region, [this](const std::wstring& ocrText) {
            if (!ocrText.empty()) {
                m_speechEngine->Speak(ocrText);
            }
            else {
                m_speechEngine->Speak(L"No text found at cursor");
            }
        });
    }
    else {
        m_speechEngine->Speak(L"No text available");
    }
}

void Controller::SpeakSelection() {
    if (!m_accessibilityReader || !m_speechEngine) return;

    m_audioFeedback->Play(Sound::SpeakStart);

    std::wstring text = m_accessibilityReader->GetSelectedText();
    if (!text.empty()) {
        m_speechEngine->Speak(text);
    }
    else {
        m_speechEngine->Speak(L"No selection");
    }
}

void Controller::StopSpeaking() {
    if (m_speechEngine) {
        m_speechEngine->Stop();
        m_audioFeedback->Play(Sound::SpeakStop);
    }
}

void Controller::ZoomIn() {
    if (!m_magnifierController) return;

    float newLevel = m_magnifierController->GetZoomLevel() * 1.25f;
    SetZoomLevel(newLevel);
}

void Controller::ZoomOut() {
    if (!m_magnifierController) return;

    float newLevel = m_magnifierController->GetZoomLevel() / 1.25f;
    SetZoomLevel(newLevel);
}

void Controller::SetZoomLevel(float level) {
    if (!m_magnifierController) return;

    // Clamp to reasonable range
    level = std::clamp(level, 1.0f, 16.0f);

    m_magnifierController->SetZoomLevel(level);

    if (level > m_magnifierController->GetZoomLevel()) {
        m_audioFeedback->Play(Sound::ZoomIn);
    }
    else {
        m_audioFeedback->Play(Sound::ZoomOut);
    }

    LOG_DEBUG("Zoom level set to {}", level);
}

void Controller::ToggleLensMode() {
    if (!m_magnifierController) return;

    bool newMode = !m_magnifierController->IsLensMode();
    m_magnifierController->SetLensMode(newMode);
    m_audioFeedback->Play(newMode ? Sound::Enable : Sound::Disable);

    LOG_INFO("Lens mode {}", newMode ? "enabled" : "disabled");
}

void Controller::CycleFollowMode() {
    if (!m_focusTracker) return;

    m_focusTracker->CycleFollowMode();
    m_audioFeedback->Play(Sound::Click);

    auto mode = m_focusTracker->GetFollowMode();
    const char* modeName = "Unknown";
    switch (mode) {
        case FollowMode::Cursor: modeName = "Cursor"; break;
        case FollowMode::Caret: modeName = "Caret"; break;
        case FollowMode::KeyboardFocus: modeName = "Focus"; break;
    }
    LOG_INFO("Follow mode changed to: {}", modeName);

    // Announce the new mode
    if (m_speechEngine) {
        m_speechEngine->Speak(std::wstring(L"Following ") +
            (mode == FollowMode::Cursor ? L"cursor" :
             mode == FollowMode::Caret ? L"caret" : L"keyboard focus"));
    }
}

} // namespace clarity
