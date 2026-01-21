/**
 * Clarity Layer - Magnifier Controller Implementation
 */

#include "MagnifierController.h"
#include "util/Logger.h"

#pragma comment(lib, "magnification.lib")

namespace clarity {

MagnifierController::MagnifierController() = default;

MagnifierController::~MagnifierController() {
    if (m_initialized) {
        Disable();
        MagUninitialize();
    }
}

bool MagnifierController::Initialize() {
    // Get virtual screen bounds
    m_screenBounds.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    m_screenBounds.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    m_screenBounds.right = m_screenBounds.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    m_screenBounds.bottom = m_screenBounds.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // Initialize Windows Magnification API
    if (MagInitialize()) {
        m_hasMagAPI = true;
        LOG_INFO("Windows Magnification API initialized");
    }
    else {
        LOG_WARN("Windows Magnification API not available, using shader-based magnification");
        m_hasMagAPI = false;
    }

    // Set initial focus to screen center
    m_focusPoint.x = (m_screenBounds.left + m_screenBounds.right) / 2;
    m_focusPoint.y = (m_screenBounds.top + m_screenBounds.bottom) / 2;

    m_initialized = true;
    LOG_INFO("MagnifierController initialized");
    return true;
}

void MagnifierController::Enable() {
    if (!m_initialized || m_enabled) return;

    m_enabled = true;
    UpdateTransform();

    LOG_INFO("Magnification enabled at {}x", m_zoomLevel);
}

void MagnifierController::Disable() {
    if (!m_enabled) return;

    m_enabled = false;

    // Reset magnification
    if (m_hasMagAPI) {
        MagSetFullscreenTransform(1.0f, 0, 0);
    }

    LOG_INFO("Magnification disabled");
}

void MagnifierController::SetZoomLevel(float level) {
    m_zoomLevel = std::clamp(level, 1.0f, 16.0f);

    if (m_enabled) {
        UpdateTransform();
    }

    LOG_DEBUG("Zoom level set to {}", m_zoomLevel);
}

void MagnifierController::SetLensMode(bool enabled) {
    m_lensMode = enabled;

    if (m_enabled) {
        UpdateTransform();
    }

    LOG_DEBUG("Lens mode {}", enabled ? "enabled" : "disabled");
}

void MagnifierController::SetLensSize(int size) {
    m_lensSize = std::clamp(size, 100, 1000);

    LOG_DEBUG("Lens size set to {}", m_lensSize);
}

void MagnifierController::SetFocusPoint(POINT pt) {
    m_focusPoint = pt;

    if (m_enabled && !m_lensMode) {
        UpdateTransform();
    }
}

void MagnifierController::OnDisplayChange() {
    // Update screen bounds
    m_screenBounds.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    m_screenBounds.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    m_screenBounds.right = m_screenBounds.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    m_screenBounds.bottom = m_screenBounds.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);

    if (m_enabled) {
        UpdateTransform();
    }

    LOG_DEBUG("Display change handled, new bounds: ({}, {}) - ({}, {})",
              m_screenBounds.left, m_screenBounds.top,
              m_screenBounds.right, m_screenBounds.bottom);
}

bool MagnifierController::SetColorEffect(const MAGCOLOREFFECT& effect) {
    if (!m_hasMagAPI) {
        LOG_WARN("Color effects require Magnification API");
        return false;
    }

    if (!MagSetFullscreenColorEffect(&effect)) {
        LOG_ERROR("Failed to set color effect");
        return false;
    }

    return true;
}

void MagnifierController::ClearColorEffect() {
    if (!m_hasMagAPI) return;

    // Identity matrix for no color effect
    MAGCOLOREFFECT identity = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f
    };

    MagSetFullscreenColorEffect(&identity);
}

void MagnifierController::UpdateTransform() {
    if (!m_enabled) return;

    if (m_lensMode) {
        // Lens mode uses shader-based magnification
        // The actual rendering is handled by the overlay system
        // Here we just need to ensure the transform is reset if using Mag API
        if (m_hasMagAPI) {
            MagSetFullscreenTransform(1.0f, 0, 0);
        }
    }
    else {
        // Full-screen magnification
        if (m_hasMagAPI) {
            int offsetX, offsetY;
            CalculatePanOffset(offsetX, offsetY);

            if (!MagSetFullscreenTransform(m_zoomLevel, offsetX, offsetY)) {
                LOG_WARN("Failed to set fullscreen transform");
            }
        }
    }
}

void MagnifierController::CalculatePanOffset(int& offsetX, int& offsetY) {
    // Calculate the visible area at current zoom
    int screenWidth = m_screenBounds.right - m_screenBounds.left;
    int screenHeight = m_screenBounds.bottom - m_screenBounds.top;

    int visibleWidth = static_cast<int>(screenWidth / m_zoomLevel);
    int visibleHeight = static_cast<int>(screenHeight / m_zoomLevel);

    // Center the visible area on the focus point
    offsetX = m_focusPoint.x - visibleWidth / 2;
    offsetY = m_focusPoint.y - visibleHeight / 2;

    // Clamp to screen bounds
    int maxOffsetX = screenWidth - visibleWidth;
    int maxOffsetY = screenHeight - visibleHeight;

    offsetX = std::clamp(offsetX, 0, std::max(0, maxOffsetX));
    offsetY = std::clamp(offsetY, 0, std::max(0, maxOffsetY));

    // Adjust for virtual screen offset
    offsetX += m_screenBounds.left;
    offsetY += m_screenBounds.top;
}

} // namespace clarity
