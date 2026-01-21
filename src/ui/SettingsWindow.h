/**
 * Clarity Layer - Settings Window
 * Main configuration UI
 */

#pragma once

#include <Windows.h>
#include <string>

namespace clarity {

// Forward declaration
class Controller;

/**
 * SettingsWindow provides the main configuration UI.
 *
 * Features:
 * - Visual settings sliders (contrast, brightness, gamma, etc.)
 * - Magnifier settings
 * - Speech settings
 * - Hotkey configuration
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

    // Hide the settings window
    void Hide();

    // Check if visible
    bool IsVisible() const { return m_visible; }

private:
    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    Controller* m_controller = nullptr;
    bool m_visible = false;

    // Window procedure
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // Create window controls
    void CreateControls();

    // Update controls from profile
    void UpdateFromProfile();

    // Save settings to profile
    void SaveToProfile();
};

} // namespace clarity
