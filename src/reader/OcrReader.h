/**
 * Clarity Layer - OCR Reader
 * Windows.Media.Ocr wrapper for text recognition
 */

#pragma once

#include <Windows.h>
#include <unknwn.h>  // Required before C++/WinRT headers for classic COM
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Storage.Streams.h>

#include <d3d11.h>
#include <string>
#include <functional>
#include <future>

namespace clarity {

/**
 * OcrReader provides OCR functionality for non-accessible content.
 *
 * Uses Windows.Media.Ocr to recognize text in screen regions.
 * This is the fallback for content that doesn't expose accessibility info.
 */
class OcrReader {
public:
    OcrReader();
    ~OcrReader();

    // Non-copyable
    OcrReader(const OcrReader&) = delete;
    OcrReader& operator=(const OcrReader&) = delete;

    // Initialize OCR engine
    bool Initialize();

    // Recognize text in a screen region (async)
    using OcrCallback = std::function<void(const std::wstring&)>;
    void RecognizeRegion(RECT region, OcrCallback callback);

    // Recognize text from D3D11 texture
    void RecognizeTexture(ID3D11Texture2D* texture, RECT region, OcrCallback callback);

    // Check if OCR is available for the current language
    bool IsAvailable() const;

    // Get supported languages
    std::vector<std::wstring> GetSupportedLanguages();

    // Set recognition language
    bool SetLanguage(const std::wstring& languageTag);

private:
    winrt::Windows::Media::Ocr::OcrEngine m_ocrEngine{nullptr};
    std::wstring m_currentLanguage;

    // Screen capture helper
    winrt::Windows::Graphics::Imaging::SoftwareBitmap CaptureRegionToBitmap(RECT region);

    // Convert D3D11 texture to SoftwareBitmap
    winrt::Windows::Graphics::Imaging::SoftwareBitmap TextureToSoftwareBitmap(
        ID3D11Texture2D* texture, RECT region);
};

} // namespace clarity
