/**
 * Clarity Layer - Profile Manager Implementation
 */

#include "ProfileManager.h"
#include "util/Logger.h"

#include <Windows.h>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace clarity {

// JSON serialization helpers
NLOHMANN_JSON_SERIALIZE_ENUM(InvertMode, {
    {InvertMode::None, "none"},
    {InvertMode::Full, "full"},
    {InvertMode::BrightnessOnly, "brightness"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(FollowMode, {
    {FollowMode::Cursor, "cursor"},
    {FollowMode::Caret, "caret"},
    {FollowMode::KeyboardFocus, "focus"},
})

// Convert wstring to string (UTF-8)
static std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

// Convert string (UTF-8) to wstring
static std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &result[0], size);
    return result;
}

ProfileManager::ProfileManager() = default;
ProfileManager::~ProfileManager() = default;

bool ProfileManager::Initialize(const std::wstring& profilesPath) {
    m_profilesPath = profilesPath;

    // Ensure directory exists
    std::error_code ec;
    std::filesystem::create_directories(m_profilesPath, ec);
    if (ec) {
        LOG_ERROR("Failed to create profiles directory: {}", ec.message());
        return false;
    }

    LOG_INFO("ProfileManager initialized: {}", m_profilesPath.string());
    return true;
}

bool ProfileManager::LoadProfile(const std::wstring& name) {
    auto path = GetProfilePath(name);

    if (!std::filesystem::exists(path)) {
        LOG_WARN("Profile not found: {}", path.string());
        return false;
    }

    // Read file
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open profile: {}", path.string());
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    // Parse
    auto profile = DeserializeProfile(content);
    if (!profile) {
        LOG_ERROR("Failed to parse profile: {}", path.string());
        return false;
    }

    m_currentProfile = *profile;
    ValidateProfile(m_currentProfile);

    LOG_INFO("Loaded profile: {}", WideToUtf8(m_currentProfile.name));
    return true;
}

bool ProfileManager::SaveCurrentProfile() {
    if (m_currentProfile.name.empty()) {
        LOG_ERROR("Cannot save profile with empty name");
        return false;
    }

    return SaveProfileAs(m_currentProfile.name);
}

bool ProfileManager::SaveProfileAs(const std::wstring& name) {
    auto path = GetProfilePath(name);

    // Ensure name is set
    Profile toSave = m_currentProfile;
    toSave.name = name;

    std::string content = SerializeProfile(toSave);

    std::ofstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to create profile file: {}", path.string());
        return false;
    }

    file << content;
    file.close();

    LOG_INFO("Saved profile: {} -> {}", WideToUtf8(name), path.string());
    return true;
}

void ProfileManager::CreateDefaultProfile() {
    m_currentProfile = Profile{};
    m_currentProfile.name = L"default";

    // High contrast preset - good starting point for low vision
    m_currentProfile.visual.enabled = false;  // Start disabled for safety
    m_currentProfile.visual.contrast = 1.5f;
    m_currentProfile.visual.brightness = 0.1f;
    m_currentProfile.visual.gamma = 1.0f;
    m_currentProfile.visual.saturation = 0.8f;
    m_currentProfile.visual.invertMode = InvertMode::None;
    m_currentProfile.visual.edgeStrength = 0.0f;

    m_currentProfile.magnifier.enabled = false;
    m_currentProfile.magnifier.zoomLevel = 2.0f;
    m_currentProfile.magnifier.followMode = FollowMode::Cursor;
    m_currentProfile.magnifier.lensMode = false;
    m_currentProfile.magnifier.lensSize = 300;

    m_currentProfile.speech.rate = 2;
    m_currentProfile.speech.volume = 100;
    m_currentProfile.speech.voice = L"default";

    // Default hotkeys
    m_currentProfile.hotkeys.toggle = L"Win+E";
    m_currentProfile.hotkeys.magnifier = L"Win+M";
    m_currentProfile.hotkeys.zoomIn = L"Win+Plus";
    m_currentProfile.hotkeys.zoomOut = L"Win+Minus";
    m_currentProfile.hotkeys.speakFocus = L"Win+F";
    m_currentProfile.hotkeys.speakCursor = L"Win+S";
    m_currentProfile.hotkeys.stopSpeaking = L"Escape";
    m_currentProfile.hotkeys.panicOff = L"Ctrl+Alt+X";

    // Save default profile
    SaveCurrentProfile();

    LOG_INFO("Created default profile");
}

std::vector<std::wstring> ProfileManager::GetProfileNames() const {
    std::vector<std::wstring> names;

    if (!std::filesystem::exists(m_profilesPath)) {
        return names;
    }

    for (const auto& entry : std::filesystem::directory_iterator(m_profilesPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            names.push_back(entry.path().stem().wstring());
        }
    }

    return names;
}

bool ProfileManager::DeleteProfile(const std::wstring& name) {
    // Don't allow deleting default profile
    if (name == L"default") {
        LOG_WARN("Cannot delete default profile");
        return false;
    }

    auto path = GetProfilePath(name);

    std::error_code ec;
    if (!std::filesystem::remove(path, ec)) {
        LOG_ERROR("Failed to delete profile: {}", path.string());
        return false;
    }

    LOG_INFO("Deleted profile: {}", WideToUtf8(name));
    return true;
}

std::filesystem::path ProfileManager::GetProfilePath(const std::wstring& name) const {
    return m_profilesPath / (name + L".json");
}

bool ProfileManager::ImportProfile(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        LOG_ERROR("Import file not found: {}", path.string());
        return false;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open import file: {}", path.string());
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    auto profile = DeserializeProfile(content);
    if (!profile) {
        LOG_ERROR("Failed to parse import file: {}", path.string());
        return false;
    }

    m_currentProfile = *profile;
    ValidateProfile(m_currentProfile);

    // Save to profiles directory
    SaveCurrentProfile();

    LOG_INFO("Imported profile: {}", WideToUtf8(m_currentProfile.name));
    return true;
}

bool ProfileManager::ExportProfile(const std::filesystem::path& path) const {
    std::string content = SerializeProfile(m_currentProfile);

    std::ofstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to create export file: {}", path.string());
        return false;
    }

    file << content;
    file.close();

    LOG_INFO("Exported profile to: {}", path.string());
    return true;
}

std::string ProfileManager::SerializeProfile(const Profile& profile) const {
    json j;

    j["name"] = WideToUtf8(profile.name);
    j["version"] = profile.version;

    // Visual settings
    j["visual"]["enabled"] = profile.visual.enabled;
    j["visual"]["contrast"] = profile.visual.contrast;
    j["visual"]["brightness"] = profile.visual.brightness;
    j["visual"]["gamma"] = profile.visual.gamma;
    j["visual"]["saturation"] = profile.visual.saturation;
    j["visual"]["invertMode"] = profile.visual.invertMode;
    j["visual"]["edgeStrength"] = profile.visual.edgeStrength;

    // Magnifier settings
    j["magnifier"]["enabled"] = profile.magnifier.enabled;
    j["magnifier"]["zoomLevel"] = profile.magnifier.zoomLevel;
    j["magnifier"]["followMode"] = profile.magnifier.followMode;
    j["magnifier"]["lensMode"] = profile.magnifier.lensMode;
    j["magnifier"]["lensSize"] = profile.magnifier.lensSize;

    // Speech settings
    j["speech"]["rate"] = profile.speech.rate;
    j["speech"]["volume"] = profile.speech.volume;
    j["speech"]["voice"] = WideToUtf8(profile.speech.voice);

    // Hotkeys
    j["hotkeys"]["toggle"] = WideToUtf8(profile.hotkeys.toggle);
    j["hotkeys"]["magnifier"] = WideToUtf8(profile.hotkeys.magnifier);
    j["hotkeys"]["zoomIn"] = WideToUtf8(profile.hotkeys.zoomIn);
    j["hotkeys"]["zoomOut"] = WideToUtf8(profile.hotkeys.zoomOut);
    j["hotkeys"]["speakFocus"] = WideToUtf8(profile.hotkeys.speakFocus);
    j["hotkeys"]["speakCursor"] = WideToUtf8(profile.hotkeys.speakCursor);
    j["hotkeys"]["stopSpeaking"] = WideToUtf8(profile.hotkeys.stopSpeaking);
    j["hotkeys"]["panicOff"] = WideToUtf8(profile.hotkeys.panicOff);

    return j.dump(2);
}

std::optional<Profile> ProfileManager::DeserializeProfile(const std::string& content) const {
    try {
        json j = json::parse(content);
        Profile profile;

        profile.name = Utf8ToWide(j.value("name", "unnamed"));
        profile.version = j.value("version", 1);

        // Visual settings
        if (j.contains("visual")) {
            auto& v = j["visual"];
            profile.visual.enabled = v.value("enabled", false);
            profile.visual.contrast = v.value("contrast", 1.0f);
            profile.visual.brightness = v.value("brightness", 0.0f);
            profile.visual.gamma = v.value("gamma", 1.0f);
            profile.visual.saturation = v.value("saturation", 1.0f);
            profile.visual.invertMode = v.value("invertMode", InvertMode::None);
            profile.visual.edgeStrength = v.value("edgeStrength", 0.0f);
        }

        // Magnifier settings
        if (j.contains("magnifier")) {
            auto& m = j["magnifier"];
            profile.magnifier.enabled = m.value("enabled", false);
            profile.magnifier.zoomLevel = m.value("zoomLevel", 2.0f);
            profile.magnifier.followMode = m.value("followMode", FollowMode::Cursor);
            profile.magnifier.lensMode = m.value("lensMode", false);
            profile.magnifier.lensSize = m.value("lensSize", 300);
        }

        // Speech settings
        if (j.contains("speech")) {
            auto& s = j["speech"];
            profile.speech.rate = s.value("rate", 0);
            profile.speech.volume = s.value("volume", 100);
            profile.speech.voice = Utf8ToWide(s.value("voice", "default"));
        }

        // Hotkeys
        if (j.contains("hotkeys")) {
            auto& h = j["hotkeys"];
            profile.hotkeys.toggle = Utf8ToWide(h.value("toggle", "Win+E"));
            profile.hotkeys.magnifier = Utf8ToWide(h.value("magnifier", "Win+M"));
            profile.hotkeys.zoomIn = Utf8ToWide(h.value("zoomIn", "Win+Plus"));
            profile.hotkeys.zoomOut = Utf8ToWide(h.value("zoomOut", "Win+Minus"));
            profile.hotkeys.speakFocus = Utf8ToWide(h.value("speakFocus", "Win+F"));
            profile.hotkeys.speakCursor = Utf8ToWide(h.value("speakCursor", "Win+S"));
            profile.hotkeys.stopSpeaking = Utf8ToWide(h.value("stopSpeaking", "Escape"));
            profile.hotkeys.panicOff = Utf8ToWide(h.value("panicOff", "Ctrl+Alt+X"));
        }

        return profile;
    }
    catch (const json::exception& e) {
        LOG_ERROR("JSON parse error: {}", e.what());
        return std::nullopt;
    }
}

void ProfileManager::ValidateProfile(Profile& profile) const {
    // Clamp visual values to valid ranges
    profile.visual.contrast = std::clamp(profile.visual.contrast, 0.0f, 4.0f);
    profile.visual.brightness = std::clamp(profile.visual.brightness, -1.0f, 1.0f);
    profile.visual.gamma = std::clamp(profile.visual.gamma, 0.1f, 4.0f);
    profile.visual.saturation = std::clamp(profile.visual.saturation, 0.0f, 2.0f);
    profile.visual.edgeStrength = std::clamp(profile.visual.edgeStrength, 0.0f, 1.0f);

    // Clamp magnifier values
    profile.magnifier.zoomLevel = std::clamp(profile.magnifier.zoomLevel, 1.0f, 16.0f);
    profile.magnifier.lensSize = std::clamp(profile.magnifier.lensSize, 100, 1000);

    // Clamp speech values
    profile.speech.rate = std::clamp(profile.speech.rate, -10, 10);
    profile.speech.volume = std::clamp(profile.speech.volume, 0, 100);

    // Ensure name is not empty
    if (profile.name.empty()) {
        profile.name = L"unnamed";
    }
}

} // namespace clarity
