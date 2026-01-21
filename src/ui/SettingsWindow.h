/**
 * Clarity Layer - Settings Window
 * Main configuration UI with live preview
 */

#pragma once

#include <Windows.h>
#include <CommCtrl.h>
#include <string>
#include <array>

namespace clarity {

// Forward declaration
class Controller;

/**
 * Control identifiers for Settings Window
 */
enum class SettingsControlID : int {
    // Tab control
    TabControl = 1000,

    // Visual tab controls (1100-1199)
    EnableEnhancementCheck = 1100,
    ContrastLabel,
    ContrastSlider,
    ContrastValue,
    BrightnessLabel,
    BrightnessSlider,
    BrightnessValue,
    GammaLabel,
    GammaSlider,
    GammaValue,
    SaturationLabel,
    SaturationSlider,
    SaturationValue,
    EdgeLabel,
    EdgeSlider,
    EdgeValue,
    InvertLabel,
    InvertCombo,

    // Magnifier tab controls (1200-1299)
    EnableMagnifierCheck = 1200,
    ZoomLabel,
    ZoomSlider,
    ZoomValue,
    FollowLabel,
    FollowCombo,
    LensModeCheck,
    LensSizeLabel,
    LensSizeSlider,
    LensSizeValue,

    // Speech tab controls (1300-1399)
    RateLabel = 1300,
    RateSlider,
    RateValue,
    VolumeLabel,
    VolumeSlider,
    VolumeValue,

    // Bottom controls (1400-1499)
    ProfileLabel = 1400,
    ProfileCombo,
    PreviewButton,
    FullScreenButton,
    ResetButton,
    MinimizeButton,
};

/**
 * SettingsWindow is the main UI for Clarity Layer.
 *
 * Features:
 * - Visual settings with live preview
 * - Preview mode (effects on part of screen)
 * - Full screen mode
 * - Profile management
 */
class SettingsWindow {
public:
    SettingsWindow();
    ~SettingsWindow();

    // Non-copyable
    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;

    // Initialize settings window
    bool Initialize(HINSTANCE hInstance, Controller* controller);

    // Show the settings window
    void Show();

    // Hide the settings window (minimize to tray)
    void Hide();

    // Check if visible
    bool IsVisible() const { return m_visible; }

    // Get window handle
    HWND GetHwnd() const { return m_hwnd; }

    // Refresh UI from current state
    void Refresh();

private:
    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    Controller* m_controller = nullptr;
    bool m_visible = false;
    bool m_initialized = false;
    bool m_previewMode = false;  // True = preview region, False = full screen

    // Tab control
    HWND m_tabControl = nullptr;
    int m_currentTab = 0;

    // Tab page windows
    HWND m_visualPage = nullptr;
    HWND m_magnifierPage = nullptr;
    HWND m_speechPage = nullptr;

    // Visual tab controls
    HWND m_enableEnhancementCheck = nullptr;
    HWND m_contrastSlider = nullptr;
    HWND m_contrastValue = nullptr;
    HWND m_brightnessSlider = nullptr;
    HWND m_brightnessValue = nullptr;
    HWND m_gammaSlider = nullptr;
    HWND m_gammaValue = nullptr;
    HWND m_saturationSlider = nullptr;
    HWND m_saturationValue = nullptr;
    HWND m_edgeSlider = nullptr;
    HWND m_edgeValue = nullptr;
    HWND m_invertCombo = nullptr;

    // Magnifier tab controls
    HWND m_enableMagnifierCheck = nullptr;
    HWND m_zoomSlider = nullptr;
    HWND m_zoomValue = nullptr;
    HWND m_followCombo = nullptr;
    HWND m_lensModeCheck = nullptr;
    HWND m_lensSizeSlider = nullptr;
    HWND m_lensSizeValue = nullptr;

    // Speech tab controls
    HWND m_rateSlider = nullptr;
    HWND m_rateValue = nullptr;
    HWND m_volumeSlider = nullptr;
    HWND m_volumeValue = nullptr;

    // Bottom controls
    HWND m_profileCombo = nullptr;
    HWND m_previewButton = nullptr;
    HWND m_fullScreenButton = nullptr;
    HWND m_resetButton = nullptr;
    HWND m_minimizeButton = nullptr;

    // Status bar
    HWND m_statusBar = nullptr;

    // Font for UI
    HFONT m_font = nullptr;
    HFONT m_boldFont = nullptr;

    // Window procedure
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // Window registration and creation
    bool RegisterWindowClass();
    bool CreateMainWindow();

    // Create UI elements
    void CreateControls();
    void CreateTabControl();
    void CreateVisualTab();
    void CreateMagnifierTab();
    void CreateSpeechTab();
    void CreateBottomControls();
    void CreateStatusBar();

    // Tab page management
    void ShowTabPage(int tabIndex);
    static LRESULT CALLBACK TabPageProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Event handlers
    void OnTabChanged();
    void OnSliderChanged(HWND slider);
    void OnComboChanged(int controlId);
    void OnCheckChanged(int controlId);
    void OnPreviewClicked();
    void OnFullScreenClicked();
    void OnResetClicked();
    void OnMinimizeClicked();

    // Update controls from profile
    void UpdateFromProfile();

    // Update status bar
    void UpdateStatus();

    // Update a value label next to a slider
    void UpdateSliderLabel(HWND slider, HWND label, const wchar_t* format, float multiplier = 1.0f);

    // Helper to create a labeled slider row
    struct SliderRow {
        HWND label;
        HWND slider;
        HWND value;
    };
    SliderRow CreateSliderRow(HWND parent, int y, const wchar_t* labelText,
                               int sliderId, int min, int max, int initial);

    // Helper to create a labeled combo row
    HWND CreateLabeledCombo(HWND parent, int y, const wchar_t* labelText, int comboId);
};

} // namespace clarity
