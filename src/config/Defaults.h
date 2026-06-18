#pragma once

#include <cstdint>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "Config.h"

constexpr uint32_t kDefaultClipDurationSec = 30;
constexpr uint32_t kDefaultBitrateMbps = 8;
constexpr uint32_t kDefaultFps = 60;
constexpr const wchar_t* kDefaultOutputDir = L"";
constexpr uint32_t kDefaultClipHotkey = VK_F10;
constexpr uint32_t kDefaultClipHotkeyMod = MOD_CONTROL | MOD_SHIFT;
constexpr EncoderType kDefaultEncoderPreference = EncoderType::Auto;
