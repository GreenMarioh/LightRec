#pragma once

#include "IFrameSource.h"
#include "CaptureConfig.h"
#include "CaptureCommon.h"
#include <atomic>
#include <mutex>

namespace LightRec::Capture {

class WGCCapture : public IFrameSource {
public:
    WGCCapture(ID3D11Device* d3d11Device, const CaptureConfig& config);
    ~WGCCapture() override;

    void start() override;
    void stop() override;
    void setCallback(FrameCallback cb) override;

private:
    void onFrameArrived(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
        winrt::Windows::Foundation::IInspectable const& args);

    ID3D11Device* d3d11Device_ = nullptr;
    CaptureConfig config_;
    FrameCallback callback_;
    std::mutex callbackMutex_;

    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item_{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool framePool_{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession session_{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::FrameArrived_revoker frameArrivedRevoker_;

    std::atomic<bool> isRunning_{ false };
};

} // namespace LightRec::Capture
