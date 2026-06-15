#pragma once
#include <windows.h>
#include <functional>
#include "UIConfig.h"

class SettingsDialog {
public:
    using ApplyCallback = std::function<void(const UIConfig&)>;

    SettingsDialog();
    ~SettingsDialog();

    // Shows the settings dialog as a modal window. Returns true if saved/applied.
    bool show(HWND parentHwnd, UIConfig& config, ApplyCallback onApply);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void createControls(HWND hwnd);
    void loadConfigValues();
    bool saveConfigValues();
    void browseOutputFolder();
    void updateControlStates();

    UIConfig* config_ = nullptr;
    ApplyCallback onApply_;
    
    HWND hwnd_ = nullptr;
    HFONT hFontNormal_ = nullptr;
    HFONT hFontBold_ = nullptr;
    HBRUSH hBackBrush_ = nullptr;
    HBRUSH hEditBrush_ = nullptr;

    // Control handles
    HWND hwndReplayCheckbox_ = nullptr;
    HWND hwndDurationCombo_ = nullptr;
    HWND hwndEncoderCombo_ = nullptr;
    HWND hwndHotkeyCtrl_ = nullptr;
    HWND hwndHotkeyShift_ = nullptr;
    HWND hwndHotkeyAlt_ = nullptr;
    HWND hwndHotkeyKeyCombo_ = nullptr;
    HWND hwndOutputDirEdit_ = nullptr;
    HWND hwndBrowseBtn_ = nullptr;
    HWND hwndOkBtn_ = nullptr;
    HWND hwndCancelBtn_ = nullptr;
    HWND hwndApplyBtn_ = nullptr;
};
