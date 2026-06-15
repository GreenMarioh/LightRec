#include "OverlayWindow.h"
#include "TrayIcon.h"
#include <windows.h>
#include <string>

OverlayWindow::OverlayWindow() {}

OverlayWindow::~OverlayWindow() {
    destroy();
}

bool OverlayWindow::create(HINSTANCE hInstance) {
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = OverlayWindow::WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"LightRecOverlayClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(20, 20, 20));
    RegisterClassExW(&wc);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int w = 260;
    int h = 105;
    int x = screenWidth - w - 20;
    int y = 40;

    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        L"LightRecOverlayClass",
        L"LightRecOverlay",
        WS_POPUP,
        x, y, w, h,
        nullptr, nullptr, hInstance, this
    );

    if (!hwnd_) return false;

    SetLayeredWindowAttributes(hwnd_, 0, 180, LWA_ALPHA);

    hFontNormal_ = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    hFontBold_ = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    return true;
}

void OverlayWindow::destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (hFontNormal_) {
        DeleteObject(hFontNormal_);
        hFontNormal_ = nullptr;
    }
    if (hFontBold_) {
        DeleteObject(hFontBold_);
        hFontBold_ = nullptr;
    }
}

void OverlayWindow::show(bool visible) {
    visible_ = visible;
    if (hwnd_) {
        ShowWindow(hwnd_, visible_ ? SW_SHOWNOACTIVATE : SW_HIDE);
    }
}

void OverlayWindow::updateStatus(RecordingState state, int fps, float ramMb, const std::wstring& lastClipPath) {
    state_ = state;
    fps_ = fps;
    ramMb_ = ramMb;
    lastClipPath_ = lastClipPath;

    if (hwnd_ && visible_) {
        InvalidateRect(hwnd_, nullptr, TRUE);
    }
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    OverlayWindow* pThis = nullptr;
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCTW* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<OverlayWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<OverlayWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->handleMessage(hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT OverlayWindow::handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            HBRUSH bgBrush = CreateSolidBrush(RGB(20, 20, 20));
            FillRect(hdc, &ps.rcPaint, bgBrush);
            DeleteObject(bgBrush);

            drawOverlay(hdc);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void OverlayWindow::drawOverlay(HDC hdc) {
    SetBkMode(hdc, TRANSPARENT);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    
    RECT rc;
    GetClientRect(hwnd_, &rc);
    MoveToEx(hdc, rc.left, rc.top, nullptr);
    LineTo(hdc, rc.right - 1, rc.top);
    LineTo(hdc, rc.right - 1, rc.bottom - 1);
    LineTo(hdc, rc.left, rc.bottom - 1);
    LineTo(hdc, rc.left, rc.top);

    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    SelectObject(hdc, hFontBold_);
    SetTextColor(hdc, RGB(255, 255, 255));
    TextOutW(hdc, 15, 12, L"LightRec Status", 15);

    SelectObject(hdc, hFontNormal_);
    SetTextColor(hdc, RGB(200, 200, 200));
    TextOutW(hdc, 15, 33, L"Replay:", 7);
    
    if (state_ == RecordingState::Recording) {
        SetTextColor(hdc, RGB(50, 205, 50));
        TextOutW(hdc, 65, 33, L"Active", 6);
    } else if (state_ == RecordingState::Clipping) {
        SetTextColor(hdc, RGB(255, 165, 0));
        TextOutW(hdc, 65, 33, L"Saving clip...", 14);
    } else {
        SetTextColor(hdc, RGB(220, 20, 60));
        TextOutW(hdc, 65, 33, L"Inactive", 8);
    }

    SetTextColor(hdc, RGB(200, 200, 200));
    std::wstring fpsStr = L"FPS: " + std::to_wstring(fps_);
    TextOutW(hdc, 15, 53, fpsStr.c_str(), static_cast<int>(fpsStr.length()));

    std::wstring ramStr = L"RAM: " + std::to_wstring(static_cast<int>(ramMb_)) + L" MB";
    TextOutW(hdc, 100, 53, ramStr.c_str(), static_cast<int>(ramStr.length()));

    SetTextColor(hdc, RGB(150, 150, 150));
    TextOutW(hdc, 15, 75, L"Last Clip:", 10);
    
    SetTextColor(hdc, RGB(255, 255, 255));
    std::wstring filename = lastClipPath_;
    size_t lastSlash = filename.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        filename = filename.substr(lastSlash + 1);
    }
    if (filename.length() > 22) {
        filename = filename.substr(0, 19) + L"...";
    }
    TextOutW(hdc, 75, 75, filename.c_str(), static_cast<int>(filename.length()));
}
