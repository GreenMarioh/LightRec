#include "HotkeyManager.h"
#include <windows.h>
#include <functional>
#include <map>
#include <string>

HotkeyManager::HotkeyManager() {}

HotkeyManager::~HotkeyManager() {
    unregisterAll();
}

void HotkeyManager::setWindow(HWND hwnd) {
    hwnd_ = hwnd;
}

bool HotkeyManager::registerHotkey(int id, UINT modifiers, UINT vk, HotkeyCallback callback) {
    unregisterHotkey(id);

    HotkeyInfo info;
    info.modifiers = modifiers;
    info.vk = vk;
    info.callback = callback;
    info.registered = false;

    UINT mods = modifiers | MOD_NOREPEAT;
    if (RegisterHotKey(hwnd_, id, mods, vk)) {
        info.registered = true;
    } else {
        if (RegisterHotKey(hwnd_, id, modifiers, vk)) {
            info.registered = true;
        }
    }

    hotkeys_[id] = info;
    return info.registered;
}

void HotkeyManager::unregisterHotkey(int id) {
    auto it = hotkeys_.find(id);
    if (it != hotkeys_.end()) {
        if (it->second.registered) {
            UnregisterHotKey(hwnd_, id);
        }
        hotkeys_.erase(it);
    }
}

void HotkeyManager::unregisterAll() {
    for (auto& [id, info] : hotkeys_) {
        if (info.registered) {
            UnregisterHotKey(hwnd_, id);
        }
    }
    hotkeys_.clear();
}

bool HotkeyManager::processMessage(const MSG& msg) {
    if (msg.message == WM_HOTKEY) {
        int id = static_cast<int>(msg.wParam);
        auto it = hotkeys_.find(id);
        if (it != hotkeys_.end() && it->second.callback) {
            it->second.callback();
            return true;
        }
    }
    return false;
}

std::wstring HotkeyManager::VirtualKeyToString(UINT vk) {
    if (vk >= 'A' && vk <= 'Z') {
        return std::wstring(1, static_cast<wchar_t>(vk));
    }
    if (vk >= '0' && vk <= '9') {
        return std::wstring(1, static_cast<wchar_t>(vk));
    }
    if (vk >= VK_F1 && vk <= VK_F24) {
        return L"F" + std::to_wstring(vk - VK_F1 + 1);
    }

    switch (vk) {
        case VK_SPACE: return L"Space";
        case VK_RETURN: return L"Enter";
        case VK_TAB: return L"Tab";
        case VK_ESCAPE: return L"Escape";
        case VK_PRIOR: return L"Page Up";
        case VK_NEXT: return L"Page Down";
        case VK_END: return L"End";
        case VK_HOME: return L"Home";
        case VK_LEFT: return L"Left Arrow";
        case VK_UP: return L"Up Arrow";
        case VK_RIGHT: return L"Right Arrow";
        case VK_DOWN: return L"Down Arrow";
        case VK_INSERT: return L"Insert";
        case VK_DELETE: return L"Delete";
        case VK_SNAPSHOT: return L"Print Screen";
        case VK_PAUSE: return L"Pause";
        default: {
            LONG scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
            LONG lParam = scanCode << 16;
            if (vk == VK_INSERT || vk == VK_DELETE || vk == VK_HOME || vk == VK_END ||
                vk == VK_PRIOR || vk == VK_NEXT || vk == VK_LEFT || vk == VK_UP ||
                vk == VK_RIGHT || vk == VK_DOWN || vk == VK_DIVIDE || vk == VK_NUMLOCK) {
                lParam |= 0x01000000;
            }
            wchar_t keyName[128] = {0};
            if (GetKeyNameTextW(lParam, keyName, 128) > 0) {
                return keyName;
            }
            wchar_t hexBuf[16];
            swprintf_s(hexBuf, L"0x%X", vk);
            return hexBuf;
        }
    }
}
