/**
 * Clarity Layer - Accessibility Reader
 * UI Automation wrapper for reading accessible content
 */

#pragma once

#include <Windows.h>
#include <UIAutomation.h>
#include <string>
#include <functional>

namespace clarity {

/**
 * AccessibilityReader uses Windows UI Automation to read
 * accessible content from applications.
 *
 * Features:
 * - Get text from focused element
 * - Get text from element at screen position
 * - Get selected text
 * - Get element properties (name, role, value)
 */
class AccessibilityReader {
public:
    AccessibilityReader();
    ~AccessibilityReader();

    // Non-copyable
    AccessibilityReader(const AccessibilityReader&) = delete;
    AccessibilityReader& operator=(const AccessibilityReader&) = delete;

    // Initialize UI Automation
    bool Initialize();

    // Get text from currently focused element
    std::wstring GetFocusedElementText();

    // Get bounding rectangle of focused element
    RECT GetFocusedElementBounds();

    // Get text from element at screen point
    std::wstring GetElementTextAtPoint(POINT pt);

    // Get currently selected text (in focused element)
    std::wstring GetSelectedText();

    // Get element name
    std::wstring GetElementName(IUIAutomationElement* element);

    // Get element control type
    std::wstring GetElementType(IUIAutomationElement* element);

    // Get element value (for editable elements)
    std::wstring GetElementValue(IUIAutomationElement* element);

    // Check if element has accessible text
    bool HasAccessibleText(IUIAutomationElement* element);

    // Register for focus changed events
    using FocusChangedCallback = std::function<void(IUIAutomationElement*)>;
    void SetFocusChangedCallback(FocusChangedCallback callback);

private:
    IUIAutomation* m_automation = nullptr;
    IUIAutomationTreeWalker* m_walker = nullptr;

    FocusChangedCallback m_focusCallback;

    // Get text content via TextPattern
    std::wstring GetTextPatternContent(IUIAutomationElement* element);

    // Get text content via ValuePattern
    std::wstring GetValuePatternContent(IUIAutomationElement* element);

    // Build readable text from element hierarchy
    std::wstring BuildAccessibleText(IUIAutomationElement* element);
};

} // namespace clarity
