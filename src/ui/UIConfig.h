#pragma once
#include <string>
#include <cstdint>

enum class EncoderType : uint32_t {
    QSV = 0,
    NVENC = 1,
    AMF = 2,
    Software = 3
};

struct UIConfig {
    bool replayEnabled = true;
    uint32_t clipDurationSec = 30;
    EncoderType preferredEncoder = EncoderType::QSV;
    std::wstring outputDir;
    bool hotkeyCtrl = true;
    bool hotkeyShift = true;
    bool hotkeyAlt = false;
    uint32_t hotkeyVk = 0x79; // VK_F10

    static std::wstring GetDefaultOutputDir();
    void load();
    void save();
};
