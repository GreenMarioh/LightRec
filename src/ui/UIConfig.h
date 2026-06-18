#pragma once
#include <string>
#include <cstdint>

#include "../config/Config.h"

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
