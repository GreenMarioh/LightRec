#pragma once
#include <windows.h>
#include <functional>
#include <map>
#include <string>

class HotkeyManager {
public:
    using HotkeyCallback = std::function<void()>;

    HotkeyManager();
    ~HotkeyManager();

    // Sets the window handle associated with hotkey registration.
    // If nullptr, hotkeys are registered globally to the calling thread.
    void setWindow(HWND hwnd);

    // Registers a hotkey. Returns true if successful.
    bool registerHotkey(int id, UINT modifiers, UINT vk, HotkeyCallback callback);
    void unregisterHotkey(int id);
    void unregisterAll();

    // Checks and dispatches a WM_HOTKEY message. Returns true if handled.
    bool processMessage(const MSG& msg);

    // Utility to get key names
    static std::wstring VirtualKeyToString(UINT vk);

private:
    HWND hwnd_ = nullptr;
    
    struct HotkeyInfo {
        UINT modifiers;
        UINT vk;
        HotkeyCallback callback;
        bool registered;
    };

    std::map<int, HotkeyInfo> hotkeys_;
};
