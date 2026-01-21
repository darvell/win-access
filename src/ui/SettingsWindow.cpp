/**
 * Clarity Layer - Settings Window Implementation
 * Main UI with enable toggles and preview mode
 */

#include "SettingsWindow.h"
#include "core/Controller.h"
#include "core/ProfileManager.h"
#include "util/Logger.h"

#include <windowsx.h>
#include <cstdio>

#pragma comment(lib, "comctl32.lib")

namespace clarity {

// Window dimensions
constexpr int WINDOW_WIDTH = 500;
constexpr int WINDOW_HEIGHT = 580;
constexpr int TAB_HEIGHT = 380;
constexpr int MARGIN = 12;
constexpr int LABEL_WIDTH = 100;
constexpr int SLIDER_WIDTH = 240;
constexpr int VALUE_WIDTH = 60;
constexpr int ROW_HEIGHT = 32;
constexpr int COMBO_WIDTH = 160;
constexpr int BUTTON_WIDTH = 100;
constexpr int BUTTON_HEIGHT = 30;

constexpr wchar_t SETTINGS_CLASS_NAME[] = L"ClaritySettingsWindow";
constexpr wchar_t TAB_PAGE_CLASS_NAME[] = L"ClaritySettingsTabPage";

SettingsWindow::SettingsWindow() = default;

SettingsWindow::~SettingsWindow() {
    if (m_boldFont) {
        DeleteObject(m_boldFont);
        m_boldFont = nullptr;
    }
    if (m_font) {
        DeleteObject(m_font);
        m_font = nullptr;
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

bool SettingsWindow::Initialize(HINSTANCE hInstance, Controller* controller) {
    m_hInstance = hInstance;
    m_controller = controller;

    // Initialize common controls (for tabs and sliders)
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_TAB_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    // Create UI fonts
    m_font = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_boldFont = CreateFontW(
        -14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    // Register window classes
    if (!RegisterWindowClass()) {
        LOG_ERROR("Failed to register settings window class");
        return false;
    }

    // Create the main window
    if (!CreateMainWindow()) {
        LOG_ERROR("Failed to create settings window");
        return false;
    }

    // Create all controls
    CreateControls();

    // Initialize from current profile
    UpdateFromProfile();
    UpdateStatus();

    m_initialized = true;
    LOG_INFO("SettingsWindow initialized");
    return true;
}

bool SettingsWindow::RegisterWindowClass() {
    // Register main window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = SETTINGS_CLASS_NAME;
    wc.hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;

    if (!RegisterClassExW(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }

    // Register tab page class
    wc.lpfnWndProc = TabPageProc;
    wc.lpszClassName = TAB_PAGE_CLASS_NAME;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);

    if (!RegisterClassExW(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }

    return true;
}

bool SettingsWindow::CreateMainWindow() {
    // Center on primary monitor
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - WINDOW_WIDTH) / 2;
    int y = (screenHeight - WINDOW_HEIGHT) / 2;

    m_hwnd = CreateWindowExW(
        WS_EX_APPWINDOW | WS_EX_CONTROLPARENT,
        SETTINGS_CLASS_NAME,
        L"Clarity Layer",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, WINDOW_WIDTH, WINDOW_HEIGHT,
        nullptr, nullptr, m_hInstance, this);

    return m_hwnd != nullptr;
}

void SettingsWindow::CreateControls() {
    CreateTabControl();
    CreateVisualTab();
    CreateMagnifierTab();
    CreateSpeechTab();
    CreateBottomControls();
    CreateStatusBar();

    // Show first tab
    ShowTabPage(0);
}

void SettingsWindow::CreateTabControl() {
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);

    m_tabControl = CreateWindowExW(
        0, WC_TABCONTROLW, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_TABS,
        MARGIN, MARGIN,
        clientRect.right - 2 * MARGIN, TAB_HEIGHT,
        m_hwnd,
        reinterpret_cast<HMENU>(static_cast<int>(SettingsControlID::TabControl)),
        m_hInstance, nullptr);

    SendMessageW(m_tabControl, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);

    // Add tabs
    TCITEMW tie = {};
    tie.mask = TCIF_TEXT;

    tie.pszText = const_cast<LPWSTR>(L"Visual");
    TabCtrl_InsertItem(m_tabControl, 0, &tie);

    tie.pszText = const_cast<LPWSTR>(L"Magnifier");
    TabCtrl_InsertItem(m_tabControl, 1, &tie);

    tie.pszText = const_cast<LPWSTR>(L"Speech");
    TabCtrl_InsertItem(m_tabControl, 2, &tie);

    // Get tab content area
    RECT tabRect;
    GetClientRect(m_tabControl, &tabRect);
    TabCtrl_AdjustRect(m_tabControl, FALSE, &tabRect);

    // Create tab pages
    int pageWidth = tabRect.right - tabRect.left;
    int pageHeight = tabRect.bottom - tabRect.top;

    m_visualPage = CreateWindowExW(
        0, TAB_PAGE_CLASS_NAME, nullptr,
        WS_CHILD | WS_CLIPSIBLINGS,
        tabRect.left, tabRect.top, pageWidth, pageHeight,
        m_tabControl, nullptr, m_hInstance, nullptr);

    m_magnifierPage = CreateWindowExW(
        0, TAB_PAGE_CLASS_NAME, nullptr,
        WS_CHILD | WS_CLIPSIBLINGS,
        tabRect.left, tabRect.top, pageWidth, pageHeight,
        m_tabControl, nullptr, m_hInstance, nullptr);

    m_speechPage = CreateWindowExW(
        0, TAB_PAGE_CLASS_NAME, nullptr,
        WS_CHILD | WS_CLIPSIBLINGS,
        tabRect.left, tabRect.top, pageWidth, pageHeight,
        m_tabControl, nullptr, m_hInstance, nullptr);
}

SettingsWindow::SliderRow SettingsWindow::CreateSliderRow(
    HWND parent, int y, const wchar_t* labelText,
    int sliderId, int min, int max, int initial) {

    SliderRow row;

    // Label
    row.label = CreateWindowExW(
        0, L"STATIC", labelText,
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        MARGIN, y, LABEL_WIDTH, 20,
        parent, nullptr, m_hInstance, nullptr);
    SendMessageW(row.label, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);

    // Slider
    row.slider = CreateWindowExW(
        0, TRACKBAR_CLASSW, nullptr,
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        MARGIN + LABEL_WIDTH + 10, y, SLIDER_WIDTH, 24,
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(sliderId)),
        m_hInstance, nullptr);
    SendMessageW(row.slider, TBM_SETRANGE, TRUE, MAKELPARAM(min, max));
    SendMessageW(row.slider, TBM_SETPOS, TRUE, initial);

    // Value label
    row.value = CreateWindowExW(
        0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        MARGIN + LABEL_WIDTH + 10 + SLIDER_WIDTH + 10, y, VALUE_WIDTH, 20,
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(sliderId + 1)),
        m_hInstance, nullptr);
    SendMessageW(row.value, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);

    return row;
}

HWND SettingsWindow::CreateLabeledCombo(HWND parent, int y, const wchar_t* labelText, int comboId) {
    // Label
    HWND label = CreateWindowExW(
        0, L"STATIC", labelText,
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        MARGIN, y, LABEL_WIDTH, 20,
        parent, nullptr, m_hInstance, nullptr);
    SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);

    // Combo box
    HWND combo = CreateWindowExW(
        0, WC_COMBOBOXW, nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        MARGIN + LABEL_WIDTH + 10, y - 2, COMBO_WIDTH, 200,
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(comboId)),
        m_hInstance, nullptr);
    SendMessageW(combo, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);

    return combo;
}

void SettingsWindow::CreateVisualTab() {
    int y = 10;

    // Enable Enhancement checkbox (prominent at top)
    m_enableEnhancementCheck = CreateWindowExW(
        0, L"BUTTON", L"Enable Visual Enhancement",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        MARGIN, y, 300, 24,
        m_visualPage,
        reinterpret_cast<HMENU>(static_cast<int>(SettingsControlID::EnableEnhancementCheck)),
        m_hInstance, nullptr);
    SendMessageW(m_enableEnhancementCheck, WM_SETFONT, reinterpret_cast<WPARAM>(m_boldFont), TRUE);
    y += ROW_HEIGHT + 5;

    // Contrast
    auto contrast = CreateSliderRow(m_visualPage, y, L"Contrast:",
        static_cast<int>(SettingsControlID::ContrastSlider), 0, 400, 100);
    m_contrastSlider = contrast.slider;
    m_contrastValue = contrast.value;
    y += ROW_HEIGHT;

    // Brightness
    auto brightness = CreateSliderRow(m_visualPage, y, L"Brightness:",
        static_cast<int>(SettingsControlID::BrightnessSlider), -100, 100, 0);
    m_brightnessSlider = brightness.slider;
    m_brightnessValue = brightness.value;
    y += ROW_HEIGHT;

    // Gamma
    auto gamma = CreateSliderRow(m_visualPage, y, L"Gamma:",
        static_cast<int>(SettingsControlID::GammaSlider), 10, 400, 100);
    m_gammaSlider = gamma.slider;
    m_gammaValue = gamma.value;
    y += ROW_HEIGHT;

    // Saturation
    auto saturation = CreateSliderRow(m_visualPage, y, L"Saturation:",
        static_cast<int>(SettingsControlID::SaturationSlider), 0, 200, 100);
    m_saturationSlider = saturation.slider;
    m_saturationValue = saturation.value;
    y += ROW_HEIGHT;

    // Edge Enhancement
    auto edge = CreateSliderRow(m_visualPage, y, L"Edge Strength:",
        static_cast<int>(SettingsControlID::EdgeSlider), 0, 100, 0);
    m_edgeSlider = edge.slider;
    m_edgeValue = edge.value;
    y += ROW_HEIGHT + 5;

    // Invert Mode combo
    m_invertCombo = CreateLabeledCombo(m_visualPage, y, L"Invert Mode:",
        static_cast<int>(SettingsControlID::InvertCombo));
    ComboBox_AddString(m_invertCombo, L"None");
    ComboBox_AddString(m_invertCombo, L"Full Invert");
    ComboBox_AddString(m_invertCombo, L"Brightness Only");
    ComboBox_SetCurSel(m_invertCombo, 0);
}

void SettingsWindow::CreateMagnifierTab() {
    int y = 10;

    // Enable Magnifier checkbox (prominent at top)
    m_enableMagnifierCheck = CreateWindowExW(
        0, L"BUTTON", L"Enable Magnifier",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        MARGIN, y, 300, 24,
        m_magnifierPage,
        reinterpret_cast<HMENU>(static_cast<int>(SettingsControlID::EnableMagnifierCheck)),
        m_hInstance, nullptr);
    SendMessageW(m_enableMagnifierCheck, WM_SETFONT, reinterpret_cast<WPARAM>(m_boldFont), TRUE);
    y += ROW_HEIGHT + 5;

    // Zoom Level
    auto zoom = CreateSliderRow(m_magnifierPage, y, L"Zoom Level:",
        static_cast<int>(SettingsControlID::ZoomSlider), 100, 1600, 200);
    m_zoomSlider = zoom.slider;
    m_zoomValue = zoom.value;
    y += ROW_HEIGHT + 5;

    // Follow Mode combo
    m_followCombo = CreateLabeledCombo(m_magnifierPage, y, L"Follow Mode:",
        static_cast<int>(SettingsControlID::FollowCombo));
    ComboBox_AddString(m_followCombo, L"Mouse Cursor");
    ComboBox_AddString(m_followCombo, L"Text Caret");
    ComboBox_AddString(m_followCombo, L"Keyboard Focus");
    ComboBox_SetCurSel(m_followCombo, 0);
    y += ROW_HEIGHT + 10;

    // Lens Mode checkbox
    m_lensModeCheck = CreateWindowExW(
        0, L"BUTTON", L"Lens Mode (magnified circle follows cursor)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        MARGIN, y, 350, 24,
        m_magnifierPage,
        reinterpret_cast<HMENU>(static_cast<int>(SettingsControlID::LensModeCheck)),
        m_hInstance, nullptr);
    SendMessageW(m_lensModeCheck, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
    y += ROW_HEIGHT;

    // Lens Size
    auto lensSize = CreateSliderRow(m_magnifierPage, y, L"Lens Size:",
        static_cast<int>(SettingsControlID::LensSizeSlider), 100, 600, 300);
    m_lensSizeSlider = lensSize.slider;
    m_lensSizeValue = lensSize.value;
}

void SettingsWindow::CreateSpeechTab() {
    int y = 10;

    // Speech Rate
    auto rate = CreateSliderRow(m_speechPage, y, L"Speech Rate:",
        static_cast<int>(SettingsControlID::RateSlider), -10, 10, 0);
    m_rateSlider = rate.slider;
    m_rateValue = rate.value;
    y += ROW_HEIGHT;

    // Volume
    auto volume = CreateSliderRow(m_speechPage, y, L"Volume:",
        static_cast<int>(SettingsControlID::VolumeSlider), 0, 100, 100);
    m_volumeSlider = volume.slider;
    m_volumeValue = volume.value;
}

void SettingsWindow::CreateBottomControls() {
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);

    int y = TAB_HEIGHT + MARGIN + 15;

    // Profile label and combo
    HWND profileLabel = CreateWindowExW(
        0, L"STATIC", L"Profile:",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        MARGIN, y + 4, 50, 20,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessageW(profileLabel, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);

    m_profileCombo = CreateWindowExW(
        0, WC_COMBOBOXW, nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        MARGIN + 55, y, 130, 200,
        m_hwnd,
        reinterpret_cast<HMENU>(static_cast<int>(SettingsControlID::ProfileCombo)),
        m_hInstance, nullptr);
    SendMessageW(m_profileCombo, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);

    // Populate profiles
    if (m_controller && m_controller->GetProfileManager()) {
        auto profiles = m_controller->GetProfileManager()->GetProfileNames();
        for (const auto& name : profiles) {
            ComboBox_AddString(m_profileCombo, name.c_str());
        }
        // Select current profile
        const auto& currentProfile = m_controller->GetProfileManager()->GetCurrentProfile();
        int idx = ComboBox_FindStringExact(m_profileCombo, -1, currentProfile.name.c_str());
        if (idx != CB_ERR) {
            ComboBox_SetCurSel(m_profileCombo, idx);
        }
    }

    y += BUTTON_HEIGHT + 10;

    // Action buttons row
    int buttonX = MARGIN;

    // Preview button
    m_previewButton = CreateWindowExW(
        0, L"BUTTON", L"Preview",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        buttonX, y, BUTTON_WIDTH, BUTTON_HEIGHT,
        m_hwnd,
        reinterpret_cast<HMENU>(static_cast<int>(SettingsControlID::PreviewButton)),
        m_hInstance, nullptr);
    SendMessageW(m_previewButton, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
    buttonX += BUTTON_WIDTH + 10;

    // Full Screen button
    m_fullScreenButton = CreateWindowExW(
        0, L"BUTTON", L"Full Screen",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        buttonX, y, BUTTON_WIDTH, BUTTON_HEIGHT,
        m_hwnd,
        reinterpret_cast<HMENU>(static_cast<int>(SettingsControlID::FullScreenButton)),
        m_hInstance, nullptr);
    SendMessageW(m_fullScreenButton, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
    buttonX += BUTTON_WIDTH + 10;

    // Reset button
    m_resetButton = CreateWindowExW(
        0, L"BUTTON", L"Reset",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        buttonX, y, 80, BUTTON_HEIGHT,
        m_hwnd,
        reinterpret_cast<HMENU>(static_cast<int>(SettingsControlID::ResetButton)),
        m_hInstance, nullptr);
    SendMessageW(m_resetButton, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);

    // Minimize to tray button (right side)
    m_minimizeButton = CreateWindowExW(
        0, L"BUTTON", L"Minimize to Tray",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        clientRect.right - MARGIN - 120, y, 120, BUTTON_HEIGHT,
        m_hwnd,
        reinterpret_cast<HMENU>(static_cast<int>(SettingsControlID::MinimizeButton)),
        m_hInstance, nullptr);
    SendMessageW(m_minimizeButton, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
}

void SettingsWindow::CreateStatusBar() {
    m_statusBar = CreateWindowExW(
        0, STATUSCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessageW(m_statusBar, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
}

void SettingsWindow::Show() {
    if (!m_hwnd) {
        LOG_WARN("Settings window not created");
        return;
    }

    // Refresh from controller state before showing
    UpdateFromProfile();
    UpdateStatus();

    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    m_visible = true;
}

void SettingsWindow::Hide() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
    m_visible = false;
}

void SettingsWindow::Refresh() {
    UpdateFromProfile();
    UpdateStatus();
}

void SettingsWindow::ShowTabPage(int tabIndex) {
    ShowWindow(m_visualPage, tabIndex == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(m_magnifierPage, tabIndex == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(m_speechPage, tabIndex == 2 ? SW_SHOW : SW_HIDE);
    m_currentTab = tabIndex;
}

LRESULT CALLBACK SettingsWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsWindow* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = static_cast<SettingsWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    }
    else {
        self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->HandleMessage(msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK SettingsWindow::TabPageProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
            // Use dialog background for controls
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT SettingsWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:
            // Minimize to tray instead of closing
            Hide();
            return 0;

        case WM_DESTROY:
            m_hwnd = nullptr;
            return 0;

        case WM_NOTIFY: {
            auto* nmhdr = reinterpret_cast<NMHDR*>(lParam);
            if (nmhdr->idFrom == static_cast<UINT_PTR>(static_cast<int>(SettingsControlID::TabControl))) {
                if (nmhdr->code == TCN_SELCHANGE) {
                    OnTabChanged();
                }
            }
            break;
        }

        case WM_HSCROLL: {
            HWND slider = reinterpret_cast<HWND>(lParam);
            OnSliderChanged(slider);
            return 0;
        }

        case WM_COMMAND: {
            int controlId = LOWORD(wParam);
            int notifyCode = HIWORD(wParam);

            if (notifyCode == CBN_SELCHANGE) {
                OnComboChanged(controlId);
            }
            else if (notifyCode == BN_CLICKED) {
                if (controlId == static_cast<int>(SettingsControlID::PreviewButton)) {
                    OnPreviewClicked();
                }
                else if (controlId == static_cast<int>(SettingsControlID::FullScreenButton)) {
                    OnFullScreenClicked();
                }
                else if (controlId == static_cast<int>(SettingsControlID::ResetButton)) {
                    OnResetClicked();
                }
                else if (controlId == static_cast<int>(SettingsControlID::MinimizeButton)) {
                    OnMinimizeClicked();
                }
                else if (controlId == static_cast<int>(SettingsControlID::EnableEnhancementCheck) ||
                         controlId == static_cast<int>(SettingsControlID::EnableMagnifierCheck) ||
                         controlId == static_cast<int>(SettingsControlID::LensModeCheck)) {
                    OnCheckChanged(controlId);
                }
            }
            return 0;
        }

        case WM_CTLCOLORSTATIC:
            // Use window background for static controls in main window
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));

        default:
            break;
    }

    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

void SettingsWindow::OnTabChanged() {
    int tabIndex = TabCtrl_GetCurSel(m_tabControl);
    ShowTabPage(tabIndex);
}

void SettingsWindow::OnSliderChanged(HWND slider) {
    // Update the value label and apply the setting immediately (live preview)
    if (!m_controller || !m_controller->GetProfileManager()) return;

    auto& profile = m_controller->GetProfileManager()->GetCurrentProfileMutable();

    if (slider == m_contrastSlider) {
        int val = static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0));
        profile.visual.contrast = val / 100.0f;
        UpdateSliderLabel(slider, m_contrastValue, L"%.2f", 0.01f);
    }
    else if (slider == m_brightnessSlider) {
        int val = static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0));
        profile.visual.brightness = val / 100.0f;
        UpdateSliderLabel(slider, m_brightnessValue, L"%.2f", 0.01f);
    }
    else if (slider == m_gammaSlider) {
        int val = static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0));
        profile.visual.gamma = val / 100.0f;
        UpdateSliderLabel(slider, m_gammaValue, L"%.2f", 0.01f);
    }
    else if (slider == m_saturationSlider) {
        int val = static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0));
        profile.visual.saturation = val / 100.0f;
        UpdateSliderLabel(slider, m_saturationValue, L"%.2f", 0.01f);
    }
    else if (slider == m_edgeSlider) {
        int val = static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0));
        profile.visual.edgeStrength = val / 100.0f;
        UpdateSliderLabel(slider, m_edgeValue, L"%.2f", 0.01f);
    }
    else if (slider == m_zoomSlider) {
        int val = static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0));
        profile.magnifier.zoomLevel = val / 100.0f;
        UpdateSliderLabel(slider, m_zoomValue, L"%.1fx", 0.01f);
    }
    else if (slider == m_lensSizeSlider) {
        int val = static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0));
        profile.magnifier.lensSize = val;
        wchar_t buf[32];
        swprintf_s(buf, L"%d px", val);
        SetWindowTextW(m_lensSizeValue, buf);
    }
    else if (slider == m_rateSlider) {
        int val = static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0));
        profile.speech.rate = val;
        wchar_t buf[32];
        swprintf_s(buf, L"%d", val);
        SetWindowTextW(m_rateValue, buf);
    }
    else if (slider == m_volumeSlider) {
        int val = static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0));
        profile.speech.volume = val;
        wchar_t buf[32];
        swprintf_s(buf, L"%d%%", val);
        SetWindowTextW(m_volumeValue, buf);
    }

    // Apply changes immediately for live preview
    m_controller->ReloadCurrentProfile();
}

void SettingsWindow::OnComboChanged(int controlId) {
    if (!m_controller || !m_controller->GetProfileManager()) return;

    auto& profile = m_controller->GetProfileManager()->GetCurrentProfileMutable();

    if (controlId == static_cast<int>(SettingsControlID::InvertCombo)) {
        int sel = ComboBox_GetCurSel(m_invertCombo);
        profile.visual.invertMode = static_cast<InvertMode>(sel);
        m_controller->ReloadCurrentProfile();
    }
    else if (controlId == static_cast<int>(SettingsControlID::FollowCombo)) {
        int sel = ComboBox_GetCurSel(m_followCombo);
        profile.magnifier.followMode = static_cast<FollowMode>(sel);
        m_controller->ReloadCurrentProfile();
    }
    else if (controlId == static_cast<int>(SettingsControlID::ProfileCombo)) {
        wchar_t profileName[256];
        ComboBox_GetText(m_profileCombo, profileName, 256);
        m_controller->SwitchProfile(profileName);
        UpdateFromProfile();
        UpdateStatus();
    }
}

void SettingsWindow::OnCheckChanged(int controlId) {
    if (!m_controller) return;

    if (controlId == static_cast<int>(SettingsControlID::EnableEnhancementCheck)) {
        BOOL checked = Button_GetCheck(m_enableEnhancementCheck) == BST_CHECKED;
        m_controller->EnableEnhancement(checked ? true : false);
        UpdateStatus();
    }
    else if (controlId == static_cast<int>(SettingsControlID::EnableMagnifierCheck)) {
        BOOL checked = Button_GetCheck(m_enableMagnifierCheck) == BST_CHECKED;
        m_controller->EnableMagnifier(checked ? true : false);
        UpdateStatus();
    }
    else if (controlId == static_cast<int>(SettingsControlID::LensModeCheck)) {
        if (m_controller->GetProfileManager()) {
            auto& profile = m_controller->GetProfileManager()->GetCurrentProfileMutable();
            BOOL checked = Button_GetCheck(m_lensModeCheck) == BST_CHECKED;
            profile.magnifier.lensMode = checked ? true : false;
            m_controller->ReloadCurrentProfile();
        }
    }
}

void SettingsWindow::OnPreviewClicked() {
    if (!m_controller) return;

    // Enable enhancement if not already enabled
    if (!m_controller->IsEnhancementEnabled()) {
        m_controller->EnableEnhancement(true);
        Button_SetCheck(m_enableEnhancementCheck, BST_CHECKED);
    }

    // TODO: Could implement a preview region mode here
    // For now, just ensure enhancement is on so user can see their changes

    m_previewMode = true;
    UpdateStatus();
}

void SettingsWindow::OnFullScreenClicked() {
    if (!m_controller) return;

    // Enable full screen enhancement
    m_controller->EnableEnhancement(true);
    Button_SetCheck(m_enableEnhancementCheck, BST_CHECKED);

    m_previewMode = false;
    UpdateStatus();

    // Save settings
    if (m_controller->GetProfileManager()) {
        m_controller->GetProfileManager()->SaveCurrentProfile();
    }
}

void SettingsWindow::OnResetClicked() {
    if (!m_controller || !m_controller->GetProfileManager()) return;

    // Reset to default values
    auto& profile = m_controller->GetProfileManager()->GetCurrentProfileMutable();

    profile.visual.contrast = 1.0f;
    profile.visual.brightness = 0.0f;
    profile.visual.gamma = 1.0f;
    profile.visual.saturation = 1.0f;
    profile.visual.edgeStrength = 0.0f;
    profile.visual.invertMode = InvertMode::None;

    profile.magnifier.zoomLevel = 2.0f;
    profile.magnifier.followMode = FollowMode::Cursor;
    profile.magnifier.lensMode = false;
    profile.magnifier.lensSize = 300;

    profile.speech.rate = 0;
    profile.speech.volume = 100;

    UpdateFromProfile();
    m_controller->ReloadCurrentProfile();
}

void SettingsWindow::OnMinimizeClicked() {
    Hide();
}

void SettingsWindow::UpdateSliderLabel(HWND slider, HWND label, const wchar_t* format, float multiplier) {
    int val = static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0));
    wchar_t buf[32];
    swprintf_s(buf, format, val * multiplier);
    SetWindowTextW(label, buf);
}

void SettingsWindow::UpdateFromProfile() {
    if (!m_controller || !m_controller->GetProfileManager()) return;

    const auto& profile = m_controller->GetProfileManager()->GetCurrentProfile();

    // Update enable checkboxes from controller state (not profile)
    Button_SetCheck(m_enableEnhancementCheck,
        m_controller->IsEnhancementEnabled() ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(m_enableMagnifierCheck,
        m_controller->IsMagnifierEnabled() ? BST_CHECKED : BST_UNCHECKED);

    // Visual tab
    SendMessageW(m_contrastSlider, TBM_SETPOS, TRUE, static_cast<int>(profile.visual.contrast * 100));
    UpdateSliderLabel(m_contrastSlider, m_contrastValue, L"%.2f", 0.01f);

    SendMessageW(m_brightnessSlider, TBM_SETPOS, TRUE, static_cast<int>(profile.visual.brightness * 100));
    UpdateSliderLabel(m_brightnessSlider, m_brightnessValue, L"%.2f", 0.01f);

    SendMessageW(m_gammaSlider, TBM_SETPOS, TRUE, static_cast<int>(profile.visual.gamma * 100));
    UpdateSliderLabel(m_gammaSlider, m_gammaValue, L"%.2f", 0.01f);

    SendMessageW(m_saturationSlider, TBM_SETPOS, TRUE, static_cast<int>(profile.visual.saturation * 100));
    UpdateSliderLabel(m_saturationSlider, m_saturationValue, L"%.2f", 0.01f);

    SendMessageW(m_edgeSlider, TBM_SETPOS, TRUE, static_cast<int>(profile.visual.edgeStrength * 100));
    UpdateSliderLabel(m_edgeSlider, m_edgeValue, L"%.2f", 0.01f);

    ComboBox_SetCurSel(m_invertCombo, static_cast<int>(profile.visual.invertMode));

    // Magnifier tab
    SendMessageW(m_zoomSlider, TBM_SETPOS, TRUE, static_cast<int>(profile.magnifier.zoomLevel * 100));
    UpdateSliderLabel(m_zoomSlider, m_zoomValue, L"%.1fx", 0.01f);

    ComboBox_SetCurSel(m_followCombo, static_cast<int>(profile.magnifier.followMode));

    Button_SetCheck(m_lensModeCheck, profile.magnifier.lensMode ? BST_CHECKED : BST_UNCHECKED);

    SendMessageW(m_lensSizeSlider, TBM_SETPOS, TRUE, profile.magnifier.lensSize);
    wchar_t lensBuf[32];
    swprintf_s(lensBuf, L"%d px", profile.magnifier.lensSize);
    SetWindowTextW(m_lensSizeValue, lensBuf);

    // Speech tab
    SendMessageW(m_rateSlider, TBM_SETPOS, TRUE, profile.speech.rate);
    wchar_t rateBuf[32];
    swprintf_s(rateBuf, L"%d", profile.speech.rate);
    SetWindowTextW(m_rateValue, rateBuf);

    SendMessageW(m_volumeSlider, TBM_SETPOS, TRUE, profile.speech.volume);
    wchar_t volBuf[32];
    swprintf_s(volBuf, L"%d%%", profile.speech.volume);
    SetWindowTextW(m_volumeValue, volBuf);

    // Update profile combo selection
    int idx = ComboBox_FindStringExact(m_profileCombo, -1, profile.name.c_str());
    if (idx != CB_ERR) {
        ComboBox_SetCurSel(m_profileCombo, idx);
    }
}

void SettingsWindow::UpdateStatus() {
    if (!m_statusBar || !m_controller) return;

    std::wstring status;

    if (m_controller->IsInSafeMode()) {
        status = L"SAFE MODE - All effects disabled";
    }
    else {
        bool enhance = m_controller->IsEnhancementEnabled();
        bool mag = m_controller->IsMagnifierEnabled();

        if (enhance && mag) {
            status = L"Enhancement: ON | Magnifier: ON";
        }
        else if (enhance) {
            status = L"Enhancement: ON | Magnifier: OFF";
        }
        else if (mag) {
            status = L"Enhancement: OFF | Magnifier: ON";
        }
        else {
            status = L"All effects OFF - Check 'Enable' boxes above";
        }
    }

    SendMessageW(m_statusBar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(status.c_str()));
}

} // namespace clarity
