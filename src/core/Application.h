#pragma once

#include <atomic>
#include <memory>

#include "../config/Config.h"
#include "D3D11Device.h"
#include "Pipeline.h"
#include "../ui/TrayIcon.h"
#include "../ui/HotkeyManager.h"
#include "../ui/OverlayWindow.h"
#include "../ui/SettingsDialog.h"

namespace lightrec::core {

class Application {
public:
    Application();
    ~Application();

    // Prevent copy/move
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool init();
    int run();
    void shutdown();

private:
    Config config_;
    D3D11Device device_;
    std::unique_ptr<Pipeline> pipeline_;
    TrayIcon tray_;
    HotkeyManager hotkeys_;
    OverlayWindow overlay_;
    std::atomic<bool> running_{false};
};

} // namespace lightrec::core
