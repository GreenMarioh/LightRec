#include "SettingsDialog.h"
#include "UIConfig.h"
#include <windows.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <string>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shlwapi.lib")

// Control IDs
#define ID_REPLAY_CHECKBOX 1001
#define ID_DURATION_COMBO 1002
#define ID_ENCODER_COMBO 1003
#define ID_HOTKEY_CTRL 1004
#define ID_HOTKEY_SHIFT 1005
#define ID_HOTKEY_ALT 1006
#define ID_HOTKEY_KEY 1007
#define ID_OUTPUT_DIR 1008
#define ID_BROWSE_BTN 1009
#define ID_OK_BTN 1010
#define ID_CANCEL_BTN 1011
#define ID_APPLY_BTN 1012

// Enable modern visual styles
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

SettingsDialog::SettingsDialog()
{
    hBackBrush_ = CreateSolidBrush(RGB(28, 28, 28));
    hEditBrush_ = CreateSolidBrush(RGB(45, 45, 45));
}

SettingsDialog::~SettingsDialog()
{
    if (hBackBrush_)
        DeleteObject(hBackBrush_);
    if (hEditBrush_)
        DeleteObject(hEditBrush_);
    if (hFontNormal_)
        DeleteObject(hFontNormal_);
    if (hFontBold_)
        DeleteObject(hFontBold_);
}

LRESULT CALLBACK SettingsDialog::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SettingsDialog *pThis = nullptr;
    if (uMsg == WM_NCCREATE)
    {
        CREATESTRUCTW *pCreate = reinterpret_cast<CREATESTRUCTW *>(lParam);
        pThis = reinterpret_cast<SettingsDialog *>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    else
    {
        pThis = reinterpret_cast<SettingsDialog *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis)
    {
        pThis->hwnd_ = hwnd;
        return pThis->handleMessage(hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

bool SettingsDialog::show(HWND parentHwnd, UIConfig &config, ApplyCallback onApply)
{
    config_ = &config;
    onApply_ = onApply;

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = SettingsDialog::WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"LightRecSettingsClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = hBackBrush_;
    WNDCLASSEXW tempWc;
    if (!GetClassInfoExW(wc.hInstance, wc.lpszClassName, &tempWc)) {
        RegisterClassExW(&wc);
    }

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int clientW = 495;
    int clientH = 550; // Increased to ensure no clipping at the bottom

    RECT rc = {0, 0, clientW, clientH};
    AdjustWindowRectEx(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0);

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    int x = (screenWidth - w) / 2;
    int y = (screenHeight - h) / 2;

    if (parentHwnd && IsWindow(parentHwnd))
    {
        RECT prect;
        GetWindowRect(parentHwnd, &prect);
        int pw = prect.right - prect.left;
        int ph = prect.bottom - prect.top;

        // Only trust the parent's rect if it looks like a real, sized window.
        if (pw > 50 && ph > 50)
        {
            x = prect.left + (pw - w) / 2;
            y = prect.top + (ph - h) / 2;
        }
    }

    // Clamp to the work area of whichever monitor we're actually on,
    // so the dialog (and its title bar) can never land off-screen.
    HMONITOR hMon = (parentHwnd && IsWindow(parentHwnd))
                        ? MonitorFromWindow(parentHwnd, MONITOR_DEFAULTTONEAREST)
                        : MonitorFromPoint(POINT{x, y}, MONITOR_DEFAULTTOPRIMARY);

    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfo(hMon, &mi);

    if (x < mi.rcWork.left)
        x = mi.rcWork.left;
    if (y < mi.rcWork.top)
        y = mi.rcWork.top;
    if (x + w > mi.rcWork.right)
        x = mi.rcWork.right - w;
    if (y + h > mi.rcWork.bottom)
        y = mi.rcWork.bottom - h;

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    hwnd_ = CreateWindowExW(
        0,
        L"LightRecSettingsClass",
        L"LightRec Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        x, y, w, h,
        parentHwnd,
        nullptr,
        GetModuleHandleW(nullptr),
        this);

    if (!hwnd_)
        return false;

    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(hwnd_, 20, &useDarkMode, sizeof(useDarkMode));

    if (parentHwnd)
    {
        EnableWindow(parentHwnd, FALSE);
    }

    loadConfigValues();
    updateControlStates();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        if (!IsDialogMessageW(hwnd_, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!IsWindow(hwnd_))
        {
            break;
        }
    }

    if (parentHwnd)
    {
        EnableWindow(parentHwnd, TRUE);
        SetFocus(parentHwnd);
    }

    return true;
}

void SettingsDialog::createControls(HWND hwnd)
{
    hFontNormal_ = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    hFontBold_ = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    HWND hwndHeader = CreateWindowExW(0, L"STATIC", L"LightRec Configuration", WS_CHILD | WS_VISIBLE, 20, 20, 440, 25, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(hwndHeader, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    hwndReplayCheckbox_ = CreateWindowExW(0, L"BUTTON", L"Enable Instant Replay (Background Recording)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 60, 440, 25, hwnd, (HMENU)ID_REPLAY_CHECKBOX, nullptr, nullptr);
    SendMessageW(hwndReplayCheckbox_, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    HWND hwndClipGroup = CreateWindowExW(0, L"BUTTON", L"Clip Settings", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 20, 95, 440, 105, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(hwndClipGroup, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    HWND hwndDurLabel = CreateWindowExW(0, L"STATIC", L"Clip Duration:", WS_CHILD | WS_VISIBLE, 35, 125, 150, 20, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(hwndDurLabel, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    hwndDurationCombo_ = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 200, 122, 230, 150, hwnd, (HMENU)ID_DURATION_COMBO, nullptr, nullptr);
    SendMessageW(hwndDurationCombo_, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);
    SendMessageW(hwndDurationCombo_, CB_ADDSTRING, 0, (LPARAM)L"15 seconds");
    SendMessageW(hwndDurationCombo_, CB_ADDSTRING, 0, (LPARAM)L"30 seconds");
    SendMessageW(hwndDurationCombo_, CB_ADDSTRING, 0, (LPARAM)L"60 seconds");
    SendMessageW(hwndDurationCombo_, CB_ADDSTRING, 0, (LPARAM)L"120 seconds");

    HWND hwndEncLabel = CreateWindowExW(0, L"STATIC", L"Preferred Encoder:", WS_CHILD | WS_VISIBLE, 35, 165, 150, 20, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(hwndEncLabel, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    hwndEncoderCombo_ = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 200, 162, 230, 150, hwnd, (HMENU)ID_ENCODER_COMBO, nullptr, nullptr);
    SendMessageW(hwndEncoderCombo_, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);
    SendMessageW(hwndEncoderCombo_, CB_ADDSTRING, 0, (LPARAM)L"Auto Select");
    SendMessageW(hwndEncoderCombo_, CB_ADDSTRING, 0, (LPARAM)L"Intel QSV");
    SendMessageW(hwndEncoderCombo_, CB_ADDSTRING, 0, (LPARAM)L"NVIDIA NVENC");
    SendMessageW(hwndEncoderCombo_, CB_ADDSTRING, 0, (LPARAM)L"AMD AMF");
    SendMessageW(hwndEncoderCombo_, CB_ADDSTRING, 0, (LPARAM)L"Software Fallback (MFT)");

    HWND hwndHotkeyGroup = CreateWindowExW(0, L"BUTTON", L"Save Clip Hotkey", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 20, 210, 440, 105, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(hwndHotkeyGroup, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    HWND hwndModifiersLabel = CreateWindowExW(0, L"STATIC", L"Modifiers:", WS_CHILD | WS_VISIBLE, 35, 240, 150, 20, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(hwndModifiersLabel, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    hwndHotkeyCtrl_ = CreateWindowExW(0, L"BUTTON", L"Ctrl", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 200, 240, 60, 20, hwnd, (HMENU)ID_HOTKEY_CTRL, nullptr, nullptr);
    SendMessageW(hwndHotkeyCtrl_, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    hwndHotkeyShift_ = CreateWindowExW(0, L"BUTTON", L"Shift", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 270, 240, 60, 20, hwnd, (HMENU)ID_HOTKEY_SHIFT, nullptr, nullptr);
    SendMessageW(hwndHotkeyShift_, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    hwndHotkeyAlt_ = CreateWindowExW(0, L"BUTTON", L"Alt", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 340, 240, 60, 20, hwnd, (HMENU)ID_HOTKEY_ALT, nullptr, nullptr);
    SendMessageW(hwndHotkeyAlt_, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    HWND hwndKeyLabel = CreateWindowExW(0, L"STATIC", L"Hotkey Key:", WS_CHILD | WS_VISIBLE, 35, 280, 150, 20, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(hwndKeyLabel, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    hwndHotkeyKeyCombo_ = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 200, 277, 230, 150, hwnd, (HMENU)ID_HOTKEY_KEY, nullptr, nullptr);
    SendMessageW(hwndHotkeyKeyCombo_, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    const struct
    {
        UINT vk;
        const wchar_t *name;
    } kKeys[] = {
        {VK_F1, L"F1"}, {VK_F2, L"F2"}, {VK_F3, L"F3"}, {VK_F4, L"F4"}, {VK_F5, L"F5"}, {VK_F6, L"F6"}, {VK_F7, L"F7"}, {VK_F8, L"F8"}, {VK_F9, L"F9"}, {VK_F10, L"F10"}, {VK_F11, L"F11"}, {VK_F12, L"F12"}, {'A', L"A"}, {'B', L"B"}, {'C', L"C"}, {'D', L"D"}, {'E', L"E"}, {'F', L"F"}, {'G', L"G"}, {'H', L"H"}, {'I', L"I"}, {'J', L"J"}, {'K', L"K"}, {'L', L"L"}, {'M', L"M"}, {'N', L"N"}, {'O', L"O"}, {'P', L"P"}, {'Q', L"Q"}, {'R', L"R"}, {'S', L"S"}, {'T', L"T"}, {'U', L"U"}, {'V', L"V"}, {'W', L"W"}, {'X', L"X"}, {'Y', L"Y"}, {'Z', L"Z"}, {VK_SPACE, L"Space"}, {VK_RETURN, L"Enter"}};
    for (const auto &k : kKeys)
    {
        int idx = (int)SendMessageW(hwndHotkeyKeyCombo_, CB_ADDSTRING, 0, (LPARAM)k.name);
        SendMessageW(hwndHotkeyKeyCombo_, CB_SETITEMDATA, idx, (LPARAM)k.vk);
    }

    HWND hwndOutputDirGroup = CreateWindowExW(0, L"BUTTON", L"Output Location", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 20, 325, 440, 85, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(hwndOutputDirGroup, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    HWND hwndDirLabel = CreateWindowExW(0, L"STATIC", L"Output Directory:", WS_CHILD | WS_VISIBLE, 35, 350, 150, 18, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(hwndDirLabel, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    hwndOutputDirEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY, 35, 372, 300, 22, hwnd, (HMENU)ID_OUTPUT_DIR, nullptr, nullptr);
    SendMessageW(hwndOutputDirEdit_, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    hwndBrowseBtn_ = CreateWindowExW(0, L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 345, 370, 85, 25, hwnd, (HMENU)ID_BROWSE_BTN, nullptr, nullptr);
    SendMessageW(hwndBrowseBtn_, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    hwndOkBtn_ = CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 205, 425, 80, 26, hwnd, (HMENU)ID_OK_BTN, nullptr, nullptr);
    SendMessageW(hwndOkBtn_, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    hwndCancelBtn_ = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 295, 425, 80, 26, hwnd, (HMENU)ID_CANCEL_BTN, nullptr, nullptr);
    SendMessageW(hwndCancelBtn_, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    hwndApplyBtn_ = CreateWindowExW(0, L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 380, 425, 80, 26, hwnd, (HMENU)ID_APPLY_BTN, nullptr, nullptr);
    SendMessageW(hwndApplyBtn_, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    SetWindowTheme(hwndDurationCombo_, L"Explorer", nullptr);
    SetWindowTheme(hwndEncoderCombo_, L"Explorer", nullptr);
    SetWindowTheme(hwndHotkeyKeyCombo_, L"Explorer", nullptr);
    SetWindowTheme(hwndOutputDirEdit_, L"Explorer", nullptr);
}

void SettingsDialog::loadConfigValues()
{
    if (!config_)
        return;

    SendMessageW(hwndReplayCheckbox_, BM_SETCHECK, config_->replayEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

    int durIdx = 1;
    if (config_->clipDurationSec == 15)
        durIdx = 0;
    else if (config_->clipDurationSec == 30)
        durIdx = 1;
    else if (config_->clipDurationSec == 60)
        durIdx = 2;
    else if (config_->clipDurationSec == 120)
        durIdx = 3;
    SendMessageW(hwndDurationCombo_, CB_SETCURSEL, durIdx, 0);

    int encIdx = static_cast<int>(config_->preferredEncoder);
    if (encIdx >= 0 && encIdx < 4)
    {
        SendMessageW(hwndEncoderCombo_, CB_SETCURSEL, encIdx, 0);
    }
    else
    {
        SendMessageW(hwndEncoderCombo_, CB_SETCURSEL, 0, 0);
    }

    SendMessageW(hwndHotkeyCtrl_, BM_SETCHECK, config_->hotkeyCtrl ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(hwndHotkeyShift_, BM_SETCHECK, config_->hotkeyShift ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(hwndHotkeyAlt_, BM_SETCHECK, config_->hotkeyAlt ? BST_CHECKED : BST_UNCHECKED, 0);

    int count = (int)SendMessageW(hwndHotkeyKeyCombo_, CB_GETCOUNT, 0, 0);
    for (int i = 0; i < count; ++i)
    {
        UINT vk = (UINT)SendMessageW(hwndHotkeyKeyCombo_, CB_GETITEMDATA, i, 0);
        if (vk == config_->hotkeyVk)
        {
            SendMessageW(hwndHotkeyKeyCombo_, CB_SETCURSEL, i, 0);
            break;
        }
    }

    SetWindowTextW(hwndOutputDirEdit_, config_->outputDir.c_str());
}

bool SettingsDialog::saveConfigValues()
{
    if (!config_)
        return false;

    bool ctrl = SendMessageW(hwndHotkeyCtrl_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool shift = SendMessageW(hwndHotkeyShift_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool alt = SendMessageW(hwndHotkeyAlt_, BM_GETCHECK, 0, 0) == BST_CHECKED;

    int keyIdx = (int)SendMessageW(hwndHotkeyKeyCombo_, CB_GETCURSEL, 0, 0);
    UINT vk = 0;
    if (keyIdx != CB_ERR)
    {
        vk = (UINT)SendMessageW(hwndHotkeyKeyCombo_, CB_GETITEMDATA, keyIdx, 0);
    }

    if (!ctrl && !shift && !alt && (vk < VK_F1 || vk > VK_F24))
    {
        MessageBoxW(hwnd_, L"Please select at least one modifier (Ctrl, Shift, or Alt) if using a standard key.", L"Invalid Hotkey", MB_OK | MB_ICONWARNING);
        return false;
    }

    config_->replayEnabled = SendMessageW(hwndReplayCheckbox_, BM_GETCHECK, 0, 0) == BST_CHECKED;

    int durIdx = (int)SendMessageW(hwndDurationCombo_, CB_GETCURSEL, 0, 0);
    if (durIdx == 0)
        config_->clipDurationSec = 15;
    else if (durIdx == 1)
        config_->clipDurationSec = 30;
    else if (durIdx == 2)
        config_->clipDurationSec = 60;
    else if (durIdx == 3)
        config_->clipDurationSec = 120;

    int encIdx = (int)SendMessageW(hwndEncoderCombo_, CB_GETCURSEL, 0, 0);
    if (encIdx != CB_ERR)
    {
        config_->preferredEncoder = static_cast<EncoderType>(encIdx);
    }

    config_->hotkeyCtrl = ctrl;
    config_->hotkeyShift = shift;
    config_->hotkeyAlt = alt;
    config_->hotkeyVk = vk;

    wchar_t buf[MAX_PATH];
    GetWindowTextW(hwndOutputDirEdit_, buf, MAX_PATH);
    config_->outputDir = buf;

    config_->save();

    if (onApply_)
    {
        onApply_(*config_);
    }

    return true;
}

void SettingsDialog::browseOutputFolder()
{
    IFileOpenDialog *pFileOpen = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileOpen))))
    {
        DWORD dwOptions;
        if (SUCCEEDED(pFileOpen->GetOptions(&dwOptions)))
        {
            pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS);
        }
        if (SUCCEEDED(pFileOpen->Show(hwnd_)))
        {
            IShellItem *pItem = nullptr;
            if (SUCCEEDED(pFileOpen->GetResult(&pItem)))
            {
                PWSTR pszPath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)))
                {
                    SetWindowTextW(hwndOutputDirEdit_, pszPath);
                    CoTaskMemFree(pszPath);
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();
    }
}

void SettingsDialog::updateControlStates()
{
    bool enabled = SendMessageW(hwndReplayCheckbox_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    EnableWindow(hwndDurationCombo_, enabled);
    EnableWindow(hwndEncoderCombo_, enabled);
    EnableWindow(hwndHotkeyCtrl_, enabled);
    EnableWindow(hwndHotkeyShift_, enabled);
    EnableWindow(hwndHotkeyAlt_, enabled);
    EnableWindow(hwndHotkeyKeyCombo_, enabled);
}

LRESULT SettingsDialog::handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        createControls(hwnd);
        return 0;

    case WM_COMMAND:
    {
        WORD id = LOWORD(wParam);
        WORD code = HIWORD(wParam);

        if (id == ID_REPLAY_CHECKBOX && code == BN_CLICKED)
        {
            updateControlStates();
        }
        else if (id == ID_BROWSE_BTN && code == BN_CLICKED)
        {
            browseOutputFolder();
        }
        else if (id == ID_OK_BTN && code == BN_CLICKED)
        {
            if (saveConfigValues())
            {
                DestroyWindow(hwnd);
            }
        }
        else if (id == ID_CANCEL_BTN && code == BN_CLICKED)
        {
            DestroyWindow(hwnd);
        }
        else if (id == ID_APPLY_BTN && code == BN_CLICKED)
        {
            saveConfigValues();
        }
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(240, 240, 240));
        SetBkMode(hdc, TRANSPARENT);
        return (INT_PTR)hBackBrush_;
    }

    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(240, 240, 240));
        SetBkColor(hdc, RGB(45, 45, 45));
        return (INT_PTR)hEditBrush_;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        FillRect(hdc, &ps.rcPaint, hBackBrush_);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}
