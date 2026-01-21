/**
 * Clarity Layer - Audio Feedback
 * Sound effects for mode changes and notifications
 */

#pragma once

#include <string>
#include <unordered_map>

namespace clarity {

/**
 * Predefined sound types for different events
 */
enum class Sound {
    Enable,         // Effect enabled
    Disable,        // Effect disabled
    ZoomIn,         // Zoom level increased
    ZoomOut,        // Zoom level decreased
    ProfileSwitch,  // Profile changed
    SpeakStart,     // Speech started
    SpeakStop,      // Speech stopped
    PanicOff,       // Emergency shutdown
    Error,          // Error occurred
    Click,          // UI interaction
    Focus           // Focus changed (subtle)
};

/**
 * AudioFeedback provides audio cues for user actions.
 * This is critical for accessibility - users need audio confirmation
 * that their commands were received.
 */
class AudioFeedback {
public:
    AudioFeedback();
    ~AudioFeedback();

    // Initialize audio system and load sounds
    bool Initialize(const std::wstring& assetsPath);

    // Play a predefined sound
    void Play(Sound sound);

    // Play a custom sound file
    void PlayFile(const std::wstring& filename);

    // Stop all currently playing sounds
    void StopAll();

    // Enable/disable audio feedback
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

    // Set volume (0.0 - 1.0)
    void SetVolume(float volume);
    float GetVolume() const { return m_volume; }

private:
    bool m_enabled = true;
    float m_volume = 1.0f;
    std::wstring m_assetsPath;
    std::unordered_map<Sound, std::wstring> m_soundFiles;

    // Load sound mapping configuration
    void LoadSoundMappings();

    // Get full path for a sound
    std::wstring GetSoundPath(Sound sound) const;
};

// Global function to play panic sound (used by SafeMode)
void PlayPanicSound();

} // namespace clarity
