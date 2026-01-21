/**
 * Clarity Layer - Speech Engine Implementation
 */

#include "SpeechEngine.h"
#include "util/Logger.h"

#pragma comment(lib, "sapi.lib")

namespace clarity {

SpeechEngine::SpeechEngine() = default;

SpeechEngine::~SpeechEngine() {
    Stop();
    m_running = false;

    if (m_speechThread.joinable()) {
        m_speechThread.join();
    }

    if (m_voice) {
        m_voice->Release();
        m_voice = nullptr;
    }
}

bool SpeechEngine::Initialize() {
    HRESULT hr = CoCreateInstance(
        CLSID_SpVoice,
        nullptr,
        CLSCTX_ALL,
        IID_ISpVoice,
        reinterpret_cast<void**>(&m_voice));

    if (FAILED(hr) || !m_voice) {
        LOG_ERROR("Failed to create SAPI voice: 0x{:08X}", hr);
        return false;
    }

    // Set default rate and volume
    m_voice->SetRate(m_rate);
    m_voice->SetVolume(static_cast<USHORT>(m_volume));

    // Start speech thread
    m_running = true;
    m_speechThread = std::thread(&SpeechEngine::SpeechThreadFunc, this);

    LOG_INFO("SpeechEngine initialized");
    return true;
}

void SpeechEngine::Speak(const std::wstring& text, bool interrupt) {
    if (!m_voice) return;

    if (interrupt) {
        Stop();
    }

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_speechQueue.push(text);
    }
}

void SpeechEngine::Stop() {
    if (!m_voice) return;

    // Purge any pending speech and stop current
    m_voice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);

    // Clear queue
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        std::queue<std::wstring> empty;
        m_speechQueue.swap(empty);
    }

    LOG_DEBUG("Speech stopped");
}

void SpeechEngine::Pause() {
    if (m_voice) {
        m_voice->Pause();
        m_paused = true;
    }
}

void SpeechEngine::Resume() {
    if (m_voice) {
        m_voice->Resume();
        m_paused = false;
    }
}

bool SpeechEngine::IsSpeaking() const {
    if (!m_voice) return false;

    SPVOICESTATUS status;
    HRESULT hr = m_voice->GetStatus(&status, nullptr);
    if (FAILED(hr)) return false;

    return status.dwRunningState == SPRS_IS_SPEAKING;
}

void SpeechEngine::SetRate(int rate) {
    m_rate = std::clamp(rate, -10, 10);
    if (m_voice) {
        m_voice->SetRate(m_rate);
    }
}

void SpeechEngine::SetVolume(int volume) {
    m_volume = std::clamp(volume, 0, 100);
    if (m_voice) {
        m_voice->SetVolume(static_cast<USHORT>(m_volume));
    }
}

std::vector<SpeechEngine::VoiceInfo> SpeechEngine::GetAvailableVoices() {
    std::vector<VoiceInfo> voices;

    if (!m_voice) return voices;

    CComPtr<IEnumSpObjectTokens> enumTokens;
    HRESULT hr = SpEnumTokens(SPCAT_VOICES, nullptr, nullptr, &enumTokens);
    if (FAILED(hr)) return voices;

    ULONG count;
    enumTokens->GetCount(&count);

    for (ULONG i = 0; i < count; ++i) {
        CComPtr<ISpObjectToken> token;
        if (FAILED(enumTokens->Next(1, &token, nullptr))) continue;

        VoiceInfo info;

        // Get voice name
        LPWSTR name = nullptr;
        if (SUCCEEDED(token->GetStringValue(nullptr, &name)) && name) {
            info.name = name;
            CoTaskMemFree(name);
        }

        // Get voice ID
        LPWSTR id = nullptr;
        if (SUCCEEDED(token->GetId(&id)) && id) {
            info.id = id;
            CoTaskMemFree(id);
        }

        // Get language attribute
        CComPtr<ISpDataKey> attributes;
        if (SUCCEEDED(token->OpenKey(SPTOKENKEY_ATTRIBUTES, &attributes))) {
            LPWSTR lang = nullptr;
            if (SUCCEEDED(attributes->GetStringValue(L"Language", &lang)) && lang) {
                info.language = lang;
                CoTaskMemFree(lang);
            }
        }

        voices.push_back(info);
    }

    return voices;
}

bool SpeechEngine::SetVoice(const std::wstring& voiceName) {
    if (!m_voice) return false;

    // Find voice token by name
    CComPtr<IEnumSpObjectTokens> enumTokens;
    HRESULT hr = SpEnumTokens(SPCAT_VOICES, nullptr, nullptr, &enumTokens);
    if (FAILED(hr)) return false;

    ULONG count;
    enumTokens->GetCount(&count);

    for (ULONG i = 0; i < count; ++i) {
        CComPtr<ISpObjectToken> token;
        if (FAILED(enumTokens->Next(1, &token, nullptr))) continue;

        LPWSTR name = nullptr;
        if (SUCCEEDED(token->GetStringValue(nullptr, &name)) && name) {
            bool match = (voiceName == name);
            CoTaskMemFree(name);

            if (match) {
                hr = m_voice->SetVoice(token);
                if (SUCCEEDED(hr)) {
                    LOG_INFO("Voice set to: {}", std::string(voiceName.begin(), voiceName.end()));
                    return true;
                }
            }
        }
    }

    LOG_WARN("Voice not found: {}", std::string(voiceName.begin(), voiceName.end()));
    return false;
}

std::wstring SpeechEngine::GetCurrentVoiceName() {
    if (!m_voice) return L"";

    CComPtr<ISpObjectToken> token;
    HRESULT hr = m_voice->GetVoice(&token);
    if (FAILED(hr) || !token) return L"";

    LPWSTR name = nullptr;
    if (SUCCEEDED(token->GetStringValue(nullptr, &name)) && name) {
        std::wstring result(name);
        CoTaskMemFree(name);
        return result;
    }

    return L"";
}

void SpeechEngine::SpeechThreadFunc() {
    while (m_running) {
        std::wstring textToSpeak;

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (!m_speechQueue.empty()) {
                textToSpeak = m_speechQueue.front();
                m_speechQueue.pop();
            }
        }

        if (!textToSpeak.empty() && m_voice) {
            // Speak synchronously (in this thread)
            m_voice->Speak(textToSpeak.c_str(), SPF_DEFAULT, nullptr);
        }
        else {
            // Sleep briefly when idle
            Sleep(50);
        }
    }
}

} // namespace clarity
