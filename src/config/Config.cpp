#include "Config.h"
#include "Defaults.h"
#include <nlohmann/json.hpp>
#include <fstream>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>

using json = nlohmann::json;

Config Config::defaults() {
    Config c;
    c.clipDurationSec = kDefaultClipDurationSec;
    c.bitrateMbps = kDefaultBitrateMbps;
    c.fps = kDefaultFps;
    c.width = 1920;
    c.height = 1080;
    c.preferredEncoder = kDefaultEncoderPreference;
    c.outputDir = kDefaultOutputDir;
    if (c.outputDir.empty()) {
        c.outputDir = defaultOutputDir();
    }
    c.clipHotkeyVk = kDefaultClipHotkey;
    c.clipHotkeyMod = kDefaultClipHotkeyMod;
    c.replayEnabled = true;
    c.systemVolume = 1.0f;
    c.micVolume = 1.0f;
    c.micEnabled = true;
    return c;
}

std::filesystem::path Config::defaultOutputDir() {
    PWSTR path = nullptr;
    std::filesystem::path defaultDir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Videos, 0, nullptr, &path))) {
        defaultDir = path;
        defaultDir /= L"LightRec";
        std::error_code ec;
        std::filesystem::create_directories(defaultDir, ec);
        CoTaskMemFree(path);
    } else {
        defaultDir = L"C:\\LightRec";
    }
    return defaultDir;
}

std::filesystem::path Config::defaultConfigPath() {
    PWSTR path = nullptr;
    std::filesystem::path configPath;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path))) {
        configPath = path;
        configPath /= L"LightRec";
        std::error_code ec;
        std::filesystem::create_directories(configPath, ec);
        configPath /= L"config.json";
        CoTaskMemFree(path);
    } else {
        configPath = L"config.json";
    }
    return configPath;
}

Config Config::load(const std::filesystem::path& path) {
    Config c = Config::defaults();
    std::ifstream file(path);
    if (file.is_open()) {
        try {
            json j;
            file >> j;
            
            if (j.contains("clipDurationSec")) c.clipDurationSec = j["clipDurationSec"];
            if (j.contains("bitrateMbps")) c.bitrateMbps = j["bitrateMbps"];
            if (j.contains("fps")) c.fps = j["fps"];
            if (j.contains("preferredEncoder")) c.preferredEncoder = static_cast<EncoderType>(j["preferredEncoder"].get<uint32_t>());
            
            if (j.contains("outputDir")) {
                std::string dirStr = j["outputDir"];
                if (dirStr.empty()) {
                    c.outputDir = defaultOutputDir();
                } else {
#ifdef __cpp_lib_char8_t
                    c.outputDir = std::filesystem::path(reinterpret_cast<const char8_t*>(dirStr.c_str()));
#else
                    c.outputDir = std::filesystem::u8path(dirStr);
#endif
                }
            }

            if (j.contains("clipHotkeyVk")) c.clipHotkeyVk = j["clipHotkeyVk"];
            if (j.contains("clipHotkeyMod")) c.clipHotkeyMod = j["clipHotkeyMod"];
            if (j.contains("replayEnabled")) c.replayEnabled = j["replayEnabled"];
            if (j.contains("systemVolume")) c.systemVolume = j["systemVolume"];
            if (j.contains("micVolume")) c.micVolume = j["micVolume"];
            if (j.contains("micEnabled")) c.micEnabled = j["micEnabled"];
        } catch (...) {
            // Ignore parse errors, fall back to whatever was parsed / defaults
        }
    }
    return c;
}

void Config::save(const std::filesystem::path& path) const {
    json j;
    j["clipDurationSec"] = clipDurationSec;
    j["bitrateMbps"] = bitrateMbps;
    j["fps"] = fps;
    j["preferredEncoder"] = static_cast<uint32_t>(preferredEncoder);
    
    auto u8str = outputDir.u8string();
    j["outputDir"] = std::string(u8str.begin(), u8str.end());

    j["clipHotkeyVk"] = clipHotkeyVk;
    j["clipHotkeyMod"] = clipHotkeyMod;
    j["replayEnabled"] = replayEnabled;
    j["systemVolume"] = systemVolume;
    j["micVolume"] = micVolume;
    j["micEnabled"] = micEnabled;

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream file(path);
    if (file.is_open()) {
        file << j.dump(4);
    }
}
