#pragma once
#include <windows.h>
#include <shellapi.h>
#include <functional>
#include <string>

enum class RecordingState {
    Idle,
    Recording,
    Clipping
};

class TrayIcon {
public:
    using Callback = std::function<void()>;
    using ToggleCallback = std::function<void(bool)>;

    TrayIcon();
    ~TrayIcon();

    // Initializes the tray icon, creating an internal message-only window
    bool create(HINSTANCE hInstance, const std::wstring& tooltip);
    void destroy();

    // Sets the recording state to update the tray icon and menu checks
    void setRecordingState(RecordingState state);
    void setTooltip(const std::wstring& tooltip);

    // Register callbacks for tray menu interactions
    void setCallbacks(ToggleCallback onToggleReplay, Callback onSaveClip, Callback onOpenSettings, Callback onOpenOverlay, Callback onExit);

    // Shows a balloon notification
    bool showBalloon(const std::wstring& title, const std::wstring& text, DWORD infoFlags = NIIF_INFO);

    // Get the internal window handle
    HWND getHwnd() const { return hwnd_; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void showContextMenu(HWND hwnd);

    HWND hwnd_ = nullptr;
    NOTIFYICONDATAW nid_ = { 0 };
    RecordingState state_ = RecordingState::Idle;
    std::wstring tooltip_;

    ToggleCallback onToggleReplay_;
    Callback onSaveClip_;
    Callback onOpenSettings_;
    Callback onOpenOverlay_;
    Callback onExit_;

    HICON hIconActive_ = nullptr;
    HICON hIconInactive_ = nullptr;
};
