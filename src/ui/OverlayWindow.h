#pragma once
#include <windows.h>
#include <string>
#include "TrayIcon.h"

class OverlayWindow {
public:
    OverlayWindow();
    ~OverlayWindow();

    // Creates the overlay window. Does not show it immediately.
    bool create(HINSTANCE hInstance);
    void destroy();

    // Controls visibility
    void show(bool visible);
    bool isVisible() const { return visible_; }
    void toggle() { show(!visible_); }

    // Update the values to display
    void updateStatus(RecordingState state, int fps, float ramMb, const std::wstring& lastClipPath);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void drawOverlay(HDC hdc);

    HWND hwnd_ = nullptr;
    bool visible_ = false;

    RecordingState state_ = RecordingState::Idle;
    int fps_ = 60;
    float ramMb_ = 32.4f;
    std::wstring lastClipPath_ = L"None";

    HFONT hFontNormal_ = nullptr;
    HFONT hFontBold_ = nullptr;
};
