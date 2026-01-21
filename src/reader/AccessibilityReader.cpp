/**
 * Clarity Layer - Accessibility Reader Implementation
 */

#include "AccessibilityReader.h"
#include "util/Logger.h"

// Note: 'interface' is defined as 'struct' via CMakeLists.txt compile definitions
// This is required for UIAutomation headers when using /permissive- flag
#include <UIAutomationClient.h>
#include <atlbase.h>
#include <sstream>

namespace clarity {

AccessibilityReader::AccessibilityReader() = default;

AccessibilityReader::~AccessibilityReader() {
    if (m_walker) {
        m_walker->Release();
        m_walker = nullptr;
    }

    if (m_automation) {
        m_automation->Release();
        m_automation = nullptr;
    }
}

bool AccessibilityReader::Initialize() {
    HRESULT hr = CoCreateInstance(
        CLSID_CUIAutomation,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IUIAutomation,
        reinterpret_cast<void**>(&m_automation));

    if (FAILED(hr) || !m_automation) {
        LOG_ERROR("Failed to create UI Automation: 0x{:08X}", hr);
        return false;
    }

    // Get content walker (skips UI chrome)
    hr = m_automation->get_ContentViewWalker(&m_walker);
    if (FAILED(hr)) {
        LOG_WARN("Failed to get content walker, using control walker");
        hr = m_automation->get_ControlViewWalker(&m_walker);
    }

    LOG_INFO("AccessibilityReader initialized");
    return true;
}

std::wstring AccessibilityReader::GetFocusedElementText() {
    if (!m_automation) return L"";

    CComPtr<IUIAutomationElement> focused;
    HRESULT hr = m_automation->GetFocusedElement(&focused);
    if (FAILED(hr) || !focused) {
        return L"";
    }

    return BuildAccessibleText(focused);
}

RECT AccessibilityReader::GetFocusedElementBounds() {
    RECT bounds = { 0, 0, 0, 0 };

    if (!m_automation) return bounds;

    CComPtr<IUIAutomationElement> focused;
    HRESULT hr = m_automation->GetFocusedElement(&focused);
    if (FAILED(hr) || !focused) {
        return bounds;
    }

    focused->get_CurrentBoundingRectangle(&bounds);
    return bounds;
}

std::wstring AccessibilityReader::GetElementTextAtPoint(POINT pt) {
    if (!m_automation) return L"";

    CComPtr<IUIAutomationElement> element;
    HRESULT hr = m_automation->ElementFromPoint(pt, &element);
    if (FAILED(hr) || !element) {
        return L"";
    }

    return BuildAccessibleText(element);
}

std::wstring AccessibilityReader::GetSelectedText() {
    if (!m_automation) return L"";

    CComPtr<IUIAutomationElement> focused;
    HRESULT hr = m_automation->GetFocusedElement(&focused);
    if (FAILED(hr) || !focused) {
        return L"";
    }

    // Try TextPattern for selection
    CComPtr<IUIAutomationTextPattern> textPattern;
    hr = focused->GetCurrentPatternAs(UIA_TextPatternId,
                                       IID_IUIAutomationTextPattern,
                                       reinterpret_cast<void**>(&textPattern));

    if (SUCCEEDED(hr) && textPattern) {
        CComPtr<IUIAutomationTextRangeArray> selection;
        hr = textPattern->GetSelection(&selection);

        if (SUCCEEDED(hr) && selection) {
            int length;
            selection->get_Length(&length);

            std::wstringstream ss;
            for (int i = 0; i < length; ++i) {
                CComPtr<IUIAutomationTextRange> range;
                selection->GetElement(i, &range);

                if (range) {
                    BSTR text;
                    range->GetText(-1, &text);
                    if (text) {
                        ss << text;
                        SysFreeString(text);
                    }
                }
            }

            return ss.str();
        }
    }

    return L"";
}

std::wstring AccessibilityReader::GetElementName(IUIAutomationElement* element) {
    if (!element) return L"";

    BSTR name;
    HRESULT hr = element->get_CurrentName(&name);
    if (SUCCEEDED(hr) && name) {
        std::wstring result(name);
        SysFreeString(name);
        return result;
    }

    return L"";
}

std::wstring AccessibilityReader::GetElementType(IUIAutomationElement* element) {
    if (!element) return L"";

    CONTROLTYPEID controlType;
    HRESULT hr = element->get_CurrentControlType(&controlType);
    if (FAILED(hr)) return L"";

    // Map control type to readable string
    switch (controlType) {
        case UIA_ButtonControlTypeId: return L"button";
        case UIA_CalendarControlTypeId: return L"calendar";
        case UIA_CheckBoxControlTypeId: return L"checkbox";
        case UIA_ComboBoxControlTypeId: return L"combo box";
        case UIA_EditControlTypeId: return L"edit";
        case UIA_HyperlinkControlTypeId: return L"link";
        case UIA_ImageControlTypeId: return L"image";
        case UIA_ListItemControlTypeId: return L"list item";
        case UIA_ListControlTypeId: return L"list";
        case UIA_MenuControlTypeId: return L"menu";
        case UIA_MenuBarControlTypeId: return L"menu bar";
        case UIA_MenuItemControlTypeId: return L"menu item";
        case UIA_ProgressBarControlTypeId: return L"progress bar";
        case UIA_RadioButtonControlTypeId: return L"radio button";
        case UIA_ScrollBarControlTypeId: return L"scroll bar";
        case UIA_SliderControlTypeId: return L"slider";
        case UIA_SpinnerControlTypeId: return L"spinner";
        case UIA_StatusBarControlTypeId: return L"status bar";
        case UIA_TabControlTypeId: return L"tab";
        case UIA_TabItemControlTypeId: return L"tab item";
        case UIA_TextControlTypeId: return L"text";
        case UIA_ToolBarControlTypeId: return L"toolbar";
        case UIA_ToolTipControlTypeId: return L"tooltip";
        case UIA_TreeControlTypeId: return L"tree";
        case UIA_TreeItemControlTypeId: return L"tree item";
        case UIA_WindowControlTypeId: return L"window";
        case UIA_DocumentControlTypeId: return L"document";
        case UIA_GroupControlTypeId: return L"group";
        default: return L"element";
    }
}

std::wstring AccessibilityReader::GetElementValue(IUIAutomationElement* element) {
    if (!element) return L"";

    // Try ValuePattern
    CComPtr<IUIAutomationValuePattern> valuePattern;
    HRESULT hr = element->GetCurrentPatternAs(UIA_ValuePatternId,
                                               IID_IUIAutomationValuePattern,
                                               reinterpret_cast<void**>(&valuePattern));

    if (SUCCEEDED(hr) && valuePattern) {
        BSTR value;
        hr = valuePattern->get_CurrentValue(&value);
        if (SUCCEEDED(hr) && value) {
            std::wstring result(value);
            SysFreeString(value);
            return result;
        }
    }

    return L"";
}

bool AccessibilityReader::HasAccessibleText(IUIAutomationElement* element) {
    if (!element) return false;

    // Check if element supports TextPattern
    CComPtr<IUnknown> pattern;
    HRESULT hr = element->GetCurrentPattern(UIA_TextPatternId, &pattern);
    if (SUCCEEDED(hr) && pattern) return true;

    // Check if element has a name
    BSTR name;
    hr = element->get_CurrentName(&name);
    if (SUCCEEDED(hr) && name && SysStringLen(name) > 0) {
        SysFreeString(name);
        return true;
    }
    if (name) SysFreeString(name);

    return false;
}

void AccessibilityReader::SetFocusChangedCallback(FocusChangedCallback callback) {
    m_focusCallback = std::move(callback);
}

std::wstring AccessibilityReader::GetTextPatternContent(IUIAutomationElement* element) {
    if (!element) return L"";

    CComPtr<IUIAutomationTextPattern> textPattern;
    HRESULT hr = element->GetCurrentPatternAs(UIA_TextPatternId,
                                               IID_IUIAutomationTextPattern,
                                               reinterpret_cast<void**>(&textPattern));

    if (SUCCEEDED(hr) && textPattern) {
        CComPtr<IUIAutomationTextRange> documentRange;
        hr = textPattern->get_DocumentRange(&documentRange);

        if (SUCCEEDED(hr) && documentRange) {
            BSTR text;
            // Limit to reasonable amount
            hr = documentRange->GetText(10000, &text);
            if (SUCCEEDED(hr) && text) {
                std::wstring result(text);
                SysFreeString(text);
                return result;
            }
        }
    }

    return L"";
}

std::wstring AccessibilityReader::GetValuePatternContent(IUIAutomationElement* element) {
    return GetElementValue(element);
}

std::wstring AccessibilityReader::BuildAccessibleText(IUIAutomationElement* element, int depth) {
    if (!element) return L"";

    // Prevent stack overflow from deep/infinite recursion
    if (depth >= MAX_RECURSION_DEPTH) {
        return L"";
    }

    std::wstringstream ss;

    // Get element type
    std::wstring type = GetElementType(element);

    // Get name
    std::wstring name = GetElementName(element);

    // Try to get text content
    std::wstring textContent = GetTextPatternContent(element);
    if (textContent.empty()) {
        textContent = GetValuePatternContent(element);
    }

    // Build readable output
    if (!name.empty()) {
        ss << name;
    }

    if (!type.empty() && type != L"element" && type != L"text") {
        if (!name.empty()) ss << L", ";
        ss << type;
    }

    if (!textContent.empty() && textContent != name) {
        if (ss.tellp() > 0) ss << L": ";
        ss << textContent;
    }

    // If we got nothing, try getting from children (with depth limit)
    if (ss.tellp() == 0 && m_walker) {
        CComPtr<IUIAutomationElement> child;
        m_walker->GetFirstChildElement(element, &child);

        if (child) {
            std::wstring childText = BuildAccessibleText(child, depth + 1);
            if (!childText.empty()) {
                ss << childText;
            }
        }
    }

    return ss.str();
}

} // namespace clarity
