#pragma once

#include <windows.h>

namespace LightRec::Capture {

enum class CaptureTargetType {
    Desktop,
    Window
};

struct CaptureConfig {
    CaptureTargetType targetType = CaptureTargetType::Desktop;
    HWND targetWindow = nullptr;
    HMONITOR targetMonitor = nullptr; // If nullptr, uses primary monitor
    bool captureCursor = true;
    bool showBorder = false;          // If false, hides WGC yellow border (Windows 10 2004+)
};

} // namespace LightRec::Capture
