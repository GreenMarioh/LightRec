#pragma once
#include <filesystem>
#include <cstdint>

enum class EncoderType {
    IntelQSV,
    NVENC,
    AMDAMF,
    SoftwareMFT
};

struct HotkeyBinding {
    uint32_t vk;
    uint32_t modifiers;
};

class Config {
public:
    uint32_t clipDurationSec = 30;
    uint32_t bitrateMbps = 8;
    uint32_t fps = 60;
    EncoderType preferredEncoder = EncoderType::IntelQSV;
    std::filesystem::path outputDir = ".";
    HotkeyBinding clipHotkey = {0, 0};

    // Frame size configs needed by the encoder
    uint32_t width = 1920;
    uint32_t height = 1080;
};
