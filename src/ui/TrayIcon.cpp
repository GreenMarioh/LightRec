#include "TrayIcon.h"
#include <windows.h>
#include <shellapi.h>
#include <string>

#define WM_TRAYICON (WM_USER + 1)
#define IDM_TOGGLE_REPLAY 2001
#define IDM_SAVE_CLIP     2002
#define IDM_SETTINGS      2003
#define IDM_OVERLAY       2004
#define IDM_EXIT          2005

TrayIcon::TrayIcon() {
    hIconInactive_ = LoadIcon(nullptr, IDI_APPLICATION);
    hIconActive_ = LoadIcon(nullptr, IDI_ERROR);
}

TrayIcon::~TrayIcon() {
    destroy();
}

bool TrayIcon::create(HINSTANCE hInstance, const std::wstring& tooltip) {
    tooltip_ = tooltip;

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = TrayIcon::WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"LightRecTrayHelperClass";
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        0,
        L"LightRecTrayHelperClass",
        L"LightRecTrayHelper",
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr, nullptr, hInstance, this
    );

    if (!hwnd_) return false;

    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = hwnd_;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    nid_.uCallbackMessage = WM_TRAYICON;
    nid_.hIcon = hIconInactive_;
    nid_.uVersion = NOTIFYICON_VERSION_4;
    
    wcscpy_s(nid_.szTip, tooltip_.c_str());

    if (!Shell_NotifyIconW(NIM_ADD, &nid_)) {
        return false;
    }

    Shell_NotifyIconW(NIM_SETVERSION, &nid_);
    setRecordingState(RecordingState::Idle);

    return true;
}

void TrayIcon::destroy() {
    if (hwnd_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void TrayIcon::setRecordingState(RecordingState state) {
    state_ = state;
    if (!hwnd_) return;

    if (state_ == RecordingState::Recording || state_ == RecordingState::Clipping) {
        nid_.hIcon = hIconActive_;
    } else {
        nid_.hIcon = hIconInactive_;
    }

    std::wstring statusTip = tooltip_;
    if (state_ == RecordingState::Recording) {
        statusTip += L" (Replay Active)";
    } else if (state_ == RecordingState::Clipping) {
        statusTip += L" (Saving Clip...)";
    } else {
        statusTip += L" (Replay Inactive)";
    }
    wcscpy_s(nid_.szTip, statusTip.c_str());

    nid_.uFlags = NIF_ICON | NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayIcon::setTooltip(const std::wstring& tooltip) {
    tooltip_ = tooltip;
    setRecordingState(state_);
}

void TrayIcon::setCallbacks(ToggleCallback onToggleReplay, Callback onSaveClip, Callback onOpenSettings, Callback onOpenOverlay, Callback onExit) {
    onToggleReplay_ = onToggleReplay;
    onSaveClip_ = onSaveClip;
    onOpenSettings_ = onOpenSettings;
    onOpenOverlay_ = onOpenOverlay;
    onExit_ = onExit;
}

bool TrayIcon::showBalloon(const std::wstring& title, const std::wstring& text, DWORD infoFlags) {
    if (!hwnd_) return false;

    NOTIFYICONDATAW balloonNid = { 0 };
    balloonNid.cbSize = sizeof(balloonNid);
    balloonNid.hWnd = hwnd_;
    balloonNid.uID = nid_.uID;
    balloonNid.uFlags = NIF_INFO;
    balloonNid.dwInfoFlags = infoFlags;
    
    wcscpy_s(balloonNid.szInfoTitle, title.c_str());
    wcscpy_s(balloonNid.szInfo, text.c_str());

    return Shell_NotifyIconW(NIM_MODIFY, &balloonNid) != FALSE;
}

LRESULT CALLBACK TrayIcon::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    TrayIcon* pThis = nullptr;
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCTW* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<TrayIcon*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<TrayIcon*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->handleMessage(hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT TrayIcon::handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_TRAYICON: {
            WORD event = LOWORD(lParam);
            if (event == WM_RBUTTONUP || event == NIN_SELECT) {
                showContextMenu(hwnd);
                return 0;
            } else if (event == WM_LBUTTONDBLCLK) {
                if (onOpenSettings_) {
                    onOpenSettings_();
                }
                return 0;
            }
            break;
        }
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case IDM_TOGGLE_REPLAY:
                    if (onToggleReplay_) {
                        onToggleReplay_(state_ == RecordingState::Idle);
                    }
                    break;
                case IDM_SAVE_CLIP:
                    if (onSaveClip_) {
                        onSaveClip_();
                    }
                    break;
                case IDM_SETTINGS:
                    if (onOpenSettings_) {
                        onOpenSettings_();
                    }
                    break;
                case IDM_OVERLAY:
                    if (onOpenOverlay_) {
                        onOpenOverlay_();
                    }
                    break;
                case IDM_EXIT:
                    if (onExit_) {
                        onExit_();
                    }
                    break;
            }
            return 0;
        }
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void TrayIcon::showContextMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    std::wstring toggleText = (state_ == RecordingState::Idle) ? L"Start Instant Replay" : L"Stop Instant Replay";
    AppendMenuW(hMenu, MF_STRING, IDM_TOGGLE_REPLAY, toggleText.c_str());
    
    UINT saveFlags = (state_ == RecordingState::Idle) ? (MF_STRING | MF_GRAYED) : MF_STRING;
    AppendMenuW(hMenu, saveFlags, IDM_SAVE_CLIP, L"Save Replay Clip\tCtrl+Shift+F10");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_SETTINGS, L"Settings...");
    AppendMenuW(hMenu, MF_STRING, IDM_OVERLAY, L"Toggle Status Overlay\tCtrl+Shift+F11");
    
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}
