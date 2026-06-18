#include "Application.h"
#include "../capture/CaptureFactory.h"
#include "../capture/CaptureConfig.h"
#include "../encoder/EncoderFactory.h"
#include "../audio/WASAPICapture.h"
#include "../ui/UIConfig.h"

namespace lightrec::core {

Application::Application() {}

Application::~Application() {
    shutdown();
}

bool Application::init() {
    // 1. Load config
    config_ = Config::load(Config::defaultConfigPath());

    // 2. Create D3D11Device
    // device_ is automatically initialized via its RAII constructor.

    // 3. Create Pipeline
    LightRec::Capture::CaptureConfig capConfig;
    auto frameSource = LightRec::Capture::CaptureFactory::create(device_.device(), capConfig);
    auto encoder = EncoderFactory::create(device_, config_);
    auto audioSource = std::make_unique<lightrec::audio::WASAPICapture>();

    pipeline_ = std::make_unique<Pipeline>(
        device_.device(),
        device_.context(),
        std::move(frameSource),
        std::move(encoder),
        std::move(audioSource),
        std::chrono::seconds(config_.clipDurationSec),
        1024 * 1024 * 64, // 64 MB video buffer
        1024 * 1024 * 16  // 16 MB audio buffer
    );

    // 4. Create TrayIcon
    HINSTANCE hInstance = GetModuleHandleW(NULL);
    if (!tray_.create(hInstance, L"LightRec")) {
        return false;
    }

    tray_.setCallbacks(
        [this](bool active) { 
            if (active) {
                if (pipeline_) pipeline_->start();
                tray_.setRecordingState(RecordingState::Recording);
            } else {
                if (pipeline_) pipeline_->stop();
                tray_.setRecordingState(RecordingState::Idle);
            }
        }, // onToggleReplay
        [this]() { if (pipeline_) pipeline_->saveClip(); }, // onSaveClip
        [this]() {
            SettingsDialog dialog;
            UIConfig uiConfig;
            uiConfig.load();
            dialog.show(tray_.getHwnd(), uiConfig, [this](const UIConfig& updated) {
                // Ignore applied config for now, in a real app this would restart pipeline
            });
        }, // onOpenSettings
        [this]() { overlay_.toggle(); }, // onOpenOverlay
        [this]() { running_ = false; PostQuitMessage(0); } // onExit
    );

    // 5. Register hotkeys
    hotkeys_.setWindow(tray_.getHwnd());
    hotkeys_.registerHotkey(1, config_.clipHotkeyMod, config_.clipHotkeyVk, [this]() {
        if (pipeline_) pipeline_->saveClip();
    });

    // 6. Create OverlayWindow
    if (!overlay_.create(hInstance)) {
        return false;
    }

    running_ = true;
    return true;
}

int Application::run() {
    if (pipeline_) {
        pipeline_->start();
    }

    tray_.setRecordingState(RecordingState::Recording);

    MSG msg = {};
    while (running_) {
        DWORD result = MsgWaitForMultipleObjects(0, nullptr, FALSE, 1000, QS_ALLINPUT);
        if (result == WAIT_OBJECT_0) {
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    running_ = false;
                    break;
                }
                if (msg.message == WM_HOTKEY) {
                    hotkeys_.processMessage(msg);
                }
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }

        if (overlay_.isVisible() && pipeline_) {
            overlay_.updateStatus(
                tray_.getRecordingState(), 
                60, 
                80.0f, 
                L"None"
            );
        }
    }

    return static_cast<int>(msg.wParam);
}

void Application::shutdown() {
    if (!running_) return;

    if (pipeline_) {
        pipeline_->stop();
    }

    tray_.destroy();
    hotkeys_.unregisterAll();
    
    config_.save(Config::defaultConfigPath());
    
    running_ = false;
}

} // namespace lightrec::core
