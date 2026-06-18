#pragma once

#include <cstdint>
#include <filesystem>

enum class EncoderType : uint32_t {
    Auto = 0,
    QSV,
    NVENC,
    AMF,
    Software
};

struct Config {
    uint32_t clipDurationSec;
    uint32_t bitrateMbps;
    uint32_t fps;
    uint32_t width;
    uint32_t height;
    EncoderType preferredEncoder;
    std::filesystem::path outputDir;
    uint32_t clipHotkeyVk;
    uint32_t clipHotkeyMod;
    bool replayEnabled;
    float systemVolume;
    float micVolume;
    bool micEnabled;

    static Config load(const std::filesystem::path& path);
    void save(const std::filesystem::path& path) const;
    static Config defaults();
    static std::filesystem::path defaultConfigPath();
    static std::filesystem::path defaultOutputDir();
};
