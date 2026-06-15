#include "WGCCapture.h"
#include <stdexcept>

namespace LightRec::Capture {

WGCCapture::WGCCapture(ID3D11Device* d3d11Device, const CaptureConfig& config)
    : d3d11Device_(d3d11Device)
    , config_(config)
{
    if (!d3d11Device_) {
        throw std::invalid_argument("d3d11Device cannot be nullptr");
    }

    if (config_.targetType == CaptureTargetType::Window) {
        if (!config_.targetWindow) {
            throw std::invalid_argument("targetWindow cannot be nullptr for window capture");
        }
        if (!IsWindow(config_.targetWindow)) {
            throw std::invalid_argument("targetWindow is not a valid HWND");
        }
        item_ = CreateCaptureItemForWindow(config_.targetWindow);
    } else {
        HMONITOR monitor = config_.targetMonitor;
        if (!monitor) {
            monitor = MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
        }
        if (!monitor) {
            throw std::runtime_error("Failed to find a valid monitor for desktop capture");
        }
        item_ = CreateCaptureItemForMonitor(monitor);
    }

    if (!item_) {
        throw std::runtime_error("Failed to create GraphicsCaptureItem");
    }
}

WGCCapture::~WGCCapture() {
    stop();
}

void WGCCapture::start() {
    if (isRunning_.exchange(true)) {
        return;
    }

    try {
        // 1. Create the WinRT Direct3DDevice wrapper
        auto winrtDevice = CreateDirect3DDevice(d3d11Device_);

        // 2. Create the Frame Pool
        framePool_ = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
            winrtDevice,
            winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            item_.Size()
        );

        // 3. Register frame arrived callback using the event revoker
        frameArrivedRevoker_ = framePool_.FrameArrived(winrt::auto_revoke, { this, &WGCCapture::onFrameArrived });

        // 4. Create the Capture Session
        session_ = framePool_.CreateCaptureSession(item_);

        // 5. Configure session properties
        session_.IsCursorEnabled(config_.captureCursor);

        // Disable border if supported (Win10 2004 / build 19041 and above)
        if (winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(
            L"Windows.Graphics.Capture.GraphicsCaptureSession", L"IsBorderRequired")) {
            session_.IsBorderRequired(config_.showBorder);
        }

        // 6. Start capturing
        session_.StartCapture();
    } catch (const winrt::hresult_error& ex) {
        isRunning_.store(false);
        throw std::runtime_error("WinRT exception during WGCCapture::start: " + winrt::to_string(ex.message()));
    } catch (const std::exception& ex) {
        isRunning_.store(false);
        throw std::runtime_error(std::string("Exception during WGCCapture::start: ") + ex.what());
    }
}

void WGCCapture::stop() {
    if (!isRunning_.exchange(false)) {
        return;
    }

    if (session_) {
        try {
            session_.Close();
        } catch (...) {}
        session_ = nullptr;
    }

    frameArrivedRevoker_.revoke();

    if (framePool_) {
        try {
            framePool_.Close();
        } catch (...) {}
        framePool_ = nullptr;
    }
}

void WGCCapture::setCallback(FrameCallback cb) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    callback_ = cb;
}

void WGCCapture::onFrameArrived(
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
    winrt::Windows::Foundation::IInspectable const& args)
{
    if (!isRunning_.load(std::memory_order_relaxed)) {
        return;
    }

    try {
        auto frame = sender.TryGetNextFrame();
        if (!frame) {
            return;
        }

        FrameCallback cb;
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            cb = callback_;
        }

        if (!cb) {
            return;
        }

        auto surface = frame.Surface();
        auto dxgiInterfaceAccess = surface.as<IDirect3DDxgiInterfaceAccess>();

        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        ThrowIfFailed(dxgiInterfaceAccess->GetInterface(IID_PPV_ARGS(&texture)), "Failed to get ID3D11Texture2D from surface");

        int64_t pts = frame.SystemRelativeTime().count();

        cb(texture.Get(), pts);
    } catch (...) {
        // Suppress errors inside the async callback thread to prevent crashes
    }
}

} // namespace LightRec::Capture
