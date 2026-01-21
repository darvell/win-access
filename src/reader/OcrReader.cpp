/**
 * Clarity Layer - OCR Reader Implementation
 */

#include "OcrReader.h"
#include "util/Logger.h"

#include <winrt/Windows.Globalization.h>
#include <inspectable.h>  // For IInspectable
#include <thread>

// IMemoryBufferByteAccess COM interface definition
// This interface provides direct byte access to memory buffers
MIDL_INTERFACE("5b0d3235-4dba-4d44-865e-8f1d0e4fd04d")
IMemoryBufferByteAccess : IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetBuffer(BYTE** value, UINT32* capacity) = 0;
};

using namespace winrt;
using namespace winrt::Windows::Media::Ocr;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Globalization;

namespace clarity {

OcrReader::OcrReader() = default;
OcrReader::~OcrReader() = default;

bool OcrReader::Initialize() {
    try {
        // Try to create OCR engine for user's language
        m_ocrEngine = OcrEngine::TryCreateFromUserProfileLanguages();

        if (!m_ocrEngine) {
            // Fall back to English
            auto englishLang = Language(L"en-US");
            if (OcrEngine::IsLanguageSupported(englishLang)) {
                m_ocrEngine = OcrEngine::TryCreateFromLanguage(englishLang);
            }
        }

        if (!m_ocrEngine) {
            LOG_ERROR("No OCR language available");
            return false;
        }

        auto lang = m_ocrEngine.RecognizerLanguage();
        m_currentLanguage = lang.LanguageTag().c_str();

        LOG_INFO("OcrReader initialized with language: {}",
                 std::string(m_currentLanguage.begin(), m_currentLanguage.end()));
        return true;
    }
    catch (const winrt::hresult_error& e) {
        LOG_ERROR("OCR initialization failed: 0x{:08X}", static_cast<uint32_t>(e.code()));
        return false;
    }
}

void OcrReader::RecognizeRegion(RECT region, OcrCallback callback) {
    if (!m_ocrEngine || !callback) return;

    // Run recognition asynchronously
    std::thread([this, region, callback]() {
        try {
            auto bitmap = CaptureRegionToBitmap(region);
            if (!bitmap) {
                callback(L"");
                return;
            }

            auto result = m_ocrEngine.RecognizeAsync(bitmap).get();

            std::wstring text;
            for (const auto& line : result.Lines()) {
                if (!text.empty()) text += L" ";
                text += line.Text().c_str();
            }

            callback(text);
        }
        catch (const winrt::hresult_error& e) {
            LOG_WARN("OCR recognition failed: 0x{:08X}", static_cast<uint32_t>(e.code()));
            callback(L"");
        }
    }).detach();
}

void OcrReader::RecognizeTexture(ID3D11Texture2D* texture, RECT region, OcrCallback callback) {
    if (!m_ocrEngine || !texture || !callback) return;

    // Run recognition asynchronously
    std::thread([this, texture, region, callback]() {
        try {
            auto bitmap = TextureToSoftwareBitmap(texture, region);
            if (!bitmap) {
                callback(L"");
                return;
            }

            auto result = m_ocrEngine.RecognizeAsync(bitmap).get();

            std::wstring text;
            for (const auto& line : result.Lines()) {
                if (!text.empty()) text += L" ";
                text += line.Text().c_str();
            }

            callback(text);
        }
        catch (const winrt::hresult_error& e) {
            LOG_WARN("OCR recognition failed: 0x{:08X}", static_cast<uint32_t>(e.code()));
            callback(L"");
        }
    }).detach();
}

bool OcrReader::IsAvailable() const {
    return m_ocrEngine != nullptr;
}

std::vector<std::wstring> OcrReader::GetSupportedLanguages() {
    std::vector<std::wstring> languages;

    for (const auto& lang : OcrEngine::AvailableRecognizerLanguages()) {
        languages.push_back(lang.LanguageTag().c_str());
    }

    return languages;
}

bool OcrReader::SetLanguage(const std::wstring& languageTag) {
    try {
        auto lang = Language(languageTag.c_str());
        if (!OcrEngine::IsLanguageSupported(lang)) {
            LOG_WARN("OCR language not supported: {}",
                     std::string(languageTag.begin(), languageTag.end()));
            return false;
        }

        m_ocrEngine = OcrEngine::TryCreateFromLanguage(lang);
        if (m_ocrEngine) {
            m_currentLanguage = languageTag;
            LOG_INFO("OCR language set to: {}",
                     std::string(languageTag.begin(), languageTag.end()));
            return true;
        }
    }
    catch (const winrt::hresult_error& e) {
        LOG_ERROR("Failed to set OCR language: 0x{:08X}", static_cast<uint32_t>(e.code()));
    }

    return false;
}

SoftwareBitmap OcrReader::CaptureRegionToBitmap(RECT region) {
    // Get screen DC
    HDC screenDC = GetDC(nullptr);
    if (!screenDC) return nullptr;

    int width = region.right - region.left;
    int height = region.bottom - region.top;

    // Create compatible DC and bitmap
    HDC memDC = CreateCompatibleDC(screenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(screenDC, width, height);
    HGDIOBJ oldBitmap = SelectObject(memDC, hBitmap);

    // Copy screen content
    BitBlt(memDC, 0, 0, width, height, screenDC, region.left, region.top, SRCCOPY);

    // Get bitmap info
    BITMAP bmpInfo;
    GetObject(hBitmap, sizeof(BITMAP), &bmpInfo);

    // Create BITMAPINFO for DIB
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;  // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    // Get pixel data
    std::vector<uint8_t> pixels(width * height * 4);
    GetDIBits(memDC, hBitmap, 0, height, pixels.data(), &bmi, DIB_RGB_COLORS);

    // Cleanup GDI
    SelectObject(memDC, oldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);

    // Create SoftwareBitmap
    try {
        SoftwareBitmap bitmap(BitmapPixelFormat::Bgra8, width, height, BitmapAlphaMode::Premultiplied);

        // Copy pixels to SoftwareBitmap
        {
            auto buffer = bitmap.LockBuffer(BitmapBufferAccessMode::Write);
            auto reference = buffer.CreateReference();

            uint8_t* dstData = nullptr;
            uint32_t capacity = 0;

            winrt::com_ptr<IMemoryBufferByteAccess> byteAccess;
            winrt::check_hresult(reference.as<IInspectable>()->QueryInterface(IID_PPV_ARGS(byteAccess.put())));
            byteAccess->GetBuffer(&dstData, &capacity);

            memcpy(dstData, pixels.data(), pixels.size());
        }

        // Convert to Gray8 for OCR if needed
        return SoftwareBitmap::Convert(bitmap, BitmapPixelFormat::Gray8);
    }
    catch (...) {
        return nullptr;
    }
}

SoftwareBitmap OcrReader::TextureToSoftwareBitmap(ID3D11Texture2D* texture, RECT region) {
    if (!texture) return nullptr;

    // Get device and context
    ID3D11Device* device = nullptr;
    texture->GetDevice(&device);
    if (!device) return nullptr;

    ID3D11DeviceContext* context = nullptr;
    device->GetImmediateContext(&context);
    if (!context) {
        device->Release();
        return nullptr;
    }

    // Get texture description
    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);

    // Create staging texture for CPU access
    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ID3D11Texture2D* stagingTexture = nullptr;
    HRESULT hr = device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
    if (FAILED(hr)) {
        context->Release();
        device->Release();
        return nullptr;
    }

    // Copy texture to staging
    context->CopyResource(stagingTexture, texture);

    // Map staging texture
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = context->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        stagingTexture->Release();
        context->Release();
        device->Release();
        return nullptr;
    }

    // Calculate region bounds
    int x = std::max(0L, region.left);
    int y = std::max(0L, region.top);
    int width = std::min(static_cast<int>(desc.Width) - x, static_cast<int>(region.right - region.left));
    int height = std::min(static_cast<int>(desc.Height) - y, static_cast<int>(region.bottom - region.top));

    if (width <= 0 || height <= 0) {
        context->Unmap(stagingTexture, 0);
        stagingTexture->Release();
        context->Release();
        device->Release();
        return nullptr;
    }

    // Extract region pixels
    std::vector<uint8_t> pixels(width * height * 4);
    uint8_t* src = static_cast<uint8_t*>(mapped.pData);
    uint8_t* dst = pixels.data();

    for (int row = 0; row < height; ++row) {
        uint8_t* srcRow = src + (y + row) * mapped.RowPitch + x * 4;
        memcpy(dst + row * width * 4, srcRow, width * 4);
    }

    context->Unmap(stagingTexture, 0);
    stagingTexture->Release();
    context->Release();
    device->Release();

    // Create SoftwareBitmap
    try {
        SoftwareBitmap bitmap(BitmapPixelFormat::Bgra8, width, height, BitmapAlphaMode::Premultiplied);

        {
            auto buffer = bitmap.LockBuffer(BitmapBufferAccessMode::Write);
            auto reference = buffer.CreateReference();

            uint8_t* dstData = nullptr;
            uint32_t capacity = 0;

            winrt::com_ptr<IMemoryBufferByteAccess> byteAccess;
            winrt::check_hresult(reference.as<IInspectable>()->QueryInterface(IID_PPV_ARGS(byteAccess.put())));
            byteAccess->GetBuffer(&dstData, &capacity);

            memcpy(dstData, pixels.data(), pixels.size());
        }

        return SoftwareBitmap::Convert(bitmap, BitmapPixelFormat::Gray8);
    }
    catch (...) {
        return nullptr;
    }
}

} // namespace clarity
