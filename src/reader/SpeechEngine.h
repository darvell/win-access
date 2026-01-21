/**
 * Clarity Layer - Speech Engine
 * SAPI Text-to-Speech wrapper
 */

#pragma once

#include <Windows.h>
#include <sapi.h>
#include <sphelper.h>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>

namespace clarity {

/**
 * SpeechEngine provides text-to-speech functionality using SAPI.
 *
 * Features:
 * - Async speech with queue management
 * - Rate and volume control
 * - Voice selection
 * - Interrupt current speech
 */
class SpeechEngine {
public:
    SpeechEngine();
    ~SpeechEngine();

    // Non-copyable
    SpeechEngine(const SpeechEngine&) = delete;
    SpeechEngine& operator=(const SpeechEngine&) = delete;

    // Initialize SAPI
    bool Initialize();

    // Speak text asynchronously
    void Speak(const std::wstring& text, bool interrupt = false);

    // Stop current speech
    void Stop();

    // Pause/resume speech
    void Pause();
    void Resume();
    bool IsPaused() const { return m_paused; }

    // Check if speaking
    bool IsSpeaking() const;

    // Rate control (-10 to 10)
    void SetRate(int rate);
    int GetRate() const { return m_rate; }

    // Volume control (0 to 100)
    void SetVolume(int volume);
    int GetVolume() const { return m_volume; }

    // Voice selection
    struct VoiceInfo {
        std::wstring name;
        std::wstring id;
        std::wstring language;
    };

    std::vector<VoiceInfo> GetAvailableVoices();
    bool SetVoice(const std::wstring& voiceName);
    std::wstring GetCurrentVoiceName();

private:
    ISpVoice* m_voice = nullptr;

    int m_rate = 0;
    int m_volume = 100;
    std::atomic<bool> m_paused{false};

    // Speech queue for managing multiple requests
    std::queue<std::wstring> m_speechQueue;
    std::mutex m_queueMutex;
    std::atomic<bool> m_running{false};
    std::thread m_speechThread;

    // Speech thread function
    void SpeechThreadFunc();
};

} // namespace clarity
