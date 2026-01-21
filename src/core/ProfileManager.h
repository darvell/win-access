/**
 * Clarity Layer - Profile Manager
 * Load, save, and switch between user profiles
 */

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace clarity {

/**
 * Inversion modes for visual transform
 */
enum class InvertMode {
    None = 0,
    Full,           // Full color inversion
    BrightnessOnly  // Invert brightness, preserve hue
};

/**
 * Follow mode for magnifier
 */
enum class FollowMode {
    Cursor,
    Caret,
    KeyboardFocus
};

/**
 * Visual enhancement settings
 */
struct VisualSettings {
    bool enabled = false;
    float contrast = 1.0f;       // 0.0 - 4.0
    float brightness = 0.0f;     // -1.0 - 1.0
    float gamma = 1.0f;          // 0.1 - 4.0
    float saturation = 1.0f;     // 0.0 - 2.0
    InvertMode invertMode = InvertMode::None;
    float edgeStrength = 0.0f;   // 0.0 - 1.0
};

/**
 * Magnifier settings
 */
struct MagnifierSettings {
    bool enabled = false;
    float zoomLevel = 2.0f;      // 1.0 - 16.0
    FollowMode followMode = FollowMode::Cursor;
    bool lensMode = false;
    int lensSize = 300;          // pixels
};

/**
 * Speech/TTS settings
 */
struct SpeechSettings {
    int rate = 0;               // -10 to 10
    int volume = 100;           // 0 to 100
    std::wstring voice;         // Voice name or "default"
};

/**
 * Hotkey configuration
 */
struct HotkeyConfig {
    std::wstring toggle = L"Win+E";
    std::wstring magnifier = L"Win+M";
    std::wstring zoomIn = L"Win+Plus";
    std::wstring zoomOut = L"Win+Minus";
    std::wstring speakFocus = L"Win+F";
    std::wstring speakCursor = L"Win+S";
    std::wstring stopSpeaking = L"Escape";
    std::wstring panicOff = L"Ctrl+Alt+X";
};

/**
 * Complete user profile
 */
struct Profile {
    std::wstring name;
    int version = 1;

    VisualSettings visual;
    MagnifierSettings magnifier;
    SpeechSettings speech;
    HotkeyConfig hotkeys;
};

/**
 * ProfileManager handles loading, saving, and switching between profiles.
 *
 * Profiles are stored as JSON files in the profiles directory.
 */
class ProfileManager {
public:
    ProfileManager();
    ~ProfileManager();

    // Initialize with profiles directory path
    bool Initialize(const std::wstring& profilesPath);

    // Load a profile by name
    bool LoadProfile(const std::wstring& name);

    // Save the current profile
    bool SaveCurrentProfile();

    // Save current profile with a new name
    bool SaveProfileAs(const std::wstring& name);

    // Create and load default profile
    void CreateDefaultProfile();

    // Get current profile
    const Profile& GetCurrentProfile() const { return m_currentProfile; }

    // Get mutable current profile (for settings UI)
    Profile& GetCurrentProfileMutable() { return m_currentProfile; }

    // Get list of available profile names
    std::vector<std::wstring> GetProfileNames() const;

    // Delete a profile
    bool DeleteProfile(const std::wstring& name);

    // Get profile path
    std::filesystem::path GetProfilePath(const std::wstring& name) const;

    // Import profile from file
    bool ImportProfile(const std::filesystem::path& path);

    // Export current profile to file
    bool ExportProfile(const std::filesystem::path& path) const;

private:
    std::filesystem::path m_profilesPath;
    Profile m_currentProfile;

    // JSON serialization
    std::string SerializeProfile(const Profile& profile) const;
    std::optional<Profile> DeserializeProfile(const std::string& json) const;

    // Validation
    void ValidateProfile(Profile& profile) const;
};

} // namespace clarity
