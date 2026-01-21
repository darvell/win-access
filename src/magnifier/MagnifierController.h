/**
 * Clarity Layer - Magnifier Controller
 * Zoom and lens mode management
 */

#pragma once

#include <Windows.h>
#include <magnification.h>
#include <functional>

namespace clarity {

/**
 * MagnifierController manages screen magnification.
 *
 * Supports two modes:
 * 1. Full-screen zoom - Magnifies entire screen with smooth panning
 * 2. Lens mode - Magnified circular/rectangular region follows focus
 *
 * Uses Windows Magnification API when available, falls back to
 * shader-based magnification otherwise.
 */
class MagnifierController {
public:
    MagnifierController();
    ~MagnifierController();

    // Non-copyable
    MagnifierController(const MagnifierController&) = delete;
    MagnifierController& operator=(const MagnifierController&) = delete;

    // Initialize magnification
    bool Initialize();

    // Enable/disable magnification
    void Enable();
    void Disable();
    bool IsEnabled() const { return m_enabled; }

    // Zoom level control
    void SetZoomLevel(float level);
    float GetZoomLevel() const { return m_zoomLevel; }

    // Lens mode
    void SetLensMode(bool enabled);
    bool IsLensMode() const { return m_lensMode; }
    void SetLensSize(int size);
    int GetLensSize() const { return m_lensSize; }

    // Set focus point (where magnification centers)
    void SetFocusPoint(POINT pt);
    POINT GetFocusPoint() const { return m_focusPoint; }

    // Handle display changes
    void OnDisplayChange();

    // Color effects (uses Magnification API)
    bool SetColorEffect(const MAGCOLOREFFECT& effect);
    void ClearColorEffect();

private:
    bool m_initialized = false;
    bool m_enabled = false;
    bool m_lensMode = false;

    float m_zoomLevel = 2.0f;
    int m_lensSize = 300;
    POINT m_focusPoint = { 0, 0 };

    // Screen bounds
    RECT m_screenBounds = {};

    // Windows Magnification API support
    bool m_hasMagAPI = false;

    // Update magnification transform
    void UpdateTransform();

    // Calculate offset for smooth panning
    void CalculatePanOffset(int& offsetX, int& offsetY);
};

} // namespace clarity
