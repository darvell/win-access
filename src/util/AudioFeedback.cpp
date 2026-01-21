/**
 * Clarity Layer - Audio Feedback Implementation
 */

#include "AudioFeedback.h"
#include "Logger.h"

#include <Windows.h>
#include <mmsystem.h>
#include <filesystem>

#pragma comment(lib, "winmm.lib")

namespace clarity {

AudioFeedback::AudioFeedback() = default;

AudioFeedback::~AudioFeedback() {
    StopAll();
}

bool AudioFeedback::Initialize(const std::wstring& assetsPath) {
    m_assetsPath = assetsPath;

    // Ensure sounds directory exists
    std::filesystem::path soundsDir = std::filesystem::path(assetsPath) / L"sounds";
    if (!std::filesystem::exists(soundsDir)) {
        LOG_WARN("Sounds directory not found: {}", soundsDir.string());
        // Not fatal - we can fall back to system sounds
    }

    LoadSoundMappings();

    LOG_INFO("AudioFeedback initialized");
    return true;
}

void AudioFeedback::LoadSoundMappings() {
    // Map sounds to files
    // These files should be short WAV files for quick playback
    m_soundFiles[Sound::Enable] = L"enable.wav";
    m_soundFiles[Sound::Disable] = L"disable.wav";
    m_soundFiles[Sound::ZoomIn] = L"zoom_in.wav";
    m_soundFiles[Sound::ZoomOut] = L"zoom_out.wav";
    m_soundFiles[Sound::ProfileSwitch] = L"profile.wav";
    m_soundFiles[Sound::SpeakStart] = L"speak_start.wav";
    m_soundFiles[Sound::SpeakStop] = L"speak_stop.wav";
    m_soundFiles[Sound::PanicOff] = L"panic.wav";
    m_soundFiles[Sound::Error] = L"error.wav";
    m_soundFiles[Sound::Click] = L"click.wav";
    m_soundFiles[Sound::Focus] = L"focus.wav";
}

void AudioFeedback::Play(Sound sound) {
    if (!m_enabled) return;

    std::wstring soundPath = GetSoundPath(sound);

    // Try to play the custom sound file first
    if (!soundPath.empty() && std::filesystem::exists(soundPath)) {
        PlayFile(soundPath);
        return;
    }

    // Fall back to system sounds
    UINT systemSound = 0;
    switch (sound) {
        case Sound::Enable:
            systemSound = MB_OK;
            break;
        case Sound::Disable:
            systemSound = MB_OK;
            break;
        case Sound::ZoomIn:
        case Sound::ZoomOut:
            systemSound = MB_OK;
            break;
        case Sound::ProfileSwitch:
            systemSound = MB_ICONASTERISK;
            break;
        case Sound::SpeakStart:
        case Sound::SpeakStop:
            // No system sound for these - they're subtle
            return;
        case Sound::PanicOff:
            systemSound = MB_ICONHAND;
            break;
        case Sound::Error:
            systemSound = MB_ICONHAND;
            break;
        case Sound::Click:
        case Sound::Focus:
            // Very subtle - no fallback
            return;
    }

    if (systemSound != 0) {
        MessageBeep(systemSound);
    }
}

void AudioFeedback::PlayFile(const std::wstring& filename) {
    if (!m_enabled) return;

    std::wstring fullPath;

    // Check if it's already a full path
    if (std::filesystem::path(filename).is_absolute()) {
        fullPath = filename;
    }
    else {
        fullPath = (std::filesystem::path(m_assetsPath) / L"sounds" / filename).wstring();
    }

    if (!std::filesystem::exists(fullPath)) {
        LOG_WARN("Sound file not found: {}", std::filesystem::path(fullPath).string());
        return;
    }

    // Play asynchronously so we don't block
    DWORD flags = SND_FILENAME | SND_ASYNC | SND_NODEFAULT;
    if (!PlaySoundW(fullPath.c_str(), nullptr, flags)) {
        LOG_WARN("Failed to play sound: {}", std::filesystem::path(fullPath).string());
    }
}

void AudioFeedback::StopAll() {
    PlaySoundW(nullptr, nullptr, 0);
}

void AudioFeedback::SetVolume(float volume) {
    m_volume = std::clamp(volume, 0.0f, 1.0f);

    // Set wave out volume
    // Volume is 0xFFFF for max, 0 for min
    // Low word is left channel, high word is right channel
    DWORD vol = static_cast<DWORD>(m_volume * 0xFFFF);
    DWORD stereoVol = vol | (vol << 16);
    waveOutSetVolume(nullptr, stereoVol);
}

std::wstring AudioFeedback::GetSoundPath(Sound sound) const {
    auto it = m_soundFiles.find(sound);
    if (it == m_soundFiles.end()) {
        return L"";
    }

    return (std::filesystem::path(m_assetsPath) / L"sounds" / it->second).wstring();
}

// Global panic sound function - must be maximally reliable
void PlayPanicSound() {
    // Method 1: System beep (most reliable)
    MessageBeep(MB_ICONHAND);

    // Method 2: Play system exclamation sound
    PlaySoundW(reinterpret_cast<LPCWSTR>(SND_ALIAS_SYSTEMEXCLAMATION),
               nullptr,
               SND_ALIAS_ID | SND_ASYNC);

    // Method 3: Beep function (hardware beep if available)
    Beep(800, 200);  // 800 Hz for 200ms
    Beep(600, 200);  // 600 Hz for 200ms
    Beep(400, 300);  // 400 Hz for 300ms (descending pattern = "off")
}

} // namespace clarity
