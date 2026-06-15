#include "UIConfig.h"
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <string>

namespace {
    std::wstring GetSettingsFilePath() {
        PWSTR path = nullptr;
        std::wstring settingsPath;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) {
            std::filesystem::path appDir(path);
            appDir /= L"LightRec";
            std::error_code ec;
            std::filesystem::create_directories(appDir, ec);
            settingsPath = (appDir / L"settings.ini").wstring();
            CoTaskMemFree(path);
        } else {
            settingsPath = L"settings.ini";
        }
        return settingsPath;
    }
}

std::wstring UIConfig::GetDefaultOutputDir() {
    PWSTR path = nullptr;
    std::wstring defaultDir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Videos, 0, nullptr, &path))) {
        defaultDir = path;
        CoTaskMemFree(path);
    } else {
        defaultDir = L"C:\\";
    }
    return defaultDir;
}

void UIConfig::load() {
    std::wstring iniPath = GetSettingsFilePath();
    
    replayEnabled = GetPrivateProfileIntW(L"Settings", L"ReplayEnabled", 1, iniPath.c_str()) != 0;
    clipDurationSec = GetPrivateProfileIntW(L"Settings", L"ClipDurationSec", 30, iniPath.c_str());
    preferredEncoder = static_cast<EncoderType>(GetPrivateProfileIntW(L"Settings", L"PreferredEncoder", 0, iniPath.c_str()));
    
    wchar_t buf[MAX_PATH];
    std::wstring defOut = GetDefaultOutputDir();
    GetPrivateProfileStringW(L"Settings", L"OutputDir", defOut.c_str(), buf, MAX_PATH, iniPath.c_str());
    outputDir = buf;

    hotkeyCtrl = GetPrivateProfileIntW(L"Settings", L"HotkeyCtrl", 1, iniPath.c_str()) != 0;
    hotkeyShift = GetPrivateProfileIntW(L"Settings", L"HotkeyShift", 1, iniPath.c_str()) != 0;
    hotkeyAlt = GetPrivateProfileIntW(L"Settings", L"HotkeyAlt", 0, iniPath.c_str()) != 0;
    hotkeyVk = GetPrivateProfileIntW(L"Settings", L"HotkeyVk", 0x79, iniPath.c_str()); // Default F10
}

void UIConfig::save() {
    std::wstring iniPath = GetSettingsFilePath();
    
    WritePrivateProfileStringW(L"Settings", L"ReplayEnabled", std::to_wstring(replayEnabled ? 1 : 0).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"ClipDurationSec", std::to_wstring(clipDurationSec).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"PreferredEncoder", std::to_wstring(static_cast<uint32_t>(preferredEncoder)).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"OutputDir", outputDir.c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"HotkeyCtrl", std::to_wstring(hotkeyCtrl ? 1 : 0).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"HotkeyShift", std::to_wstring(hotkeyShift ? 1 : 0).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"HotkeyAlt", std::to_wstring(hotkeyAlt ? 1 : 0).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"HotkeyVk", std::to_wstring(hotkeyVk).c_str(), iniPath.c_str());
}
