#pragma once

#include "IFrameSource.h"
#include "CaptureConfig.h"
#include "CaptureCommon.h"
#include <atomic>
#include <mutex>
#include <thread>

namespace LightRec::Capture {

class DXGICapture : public IFrameSource {
public:
    DXGICapture(ID3D11Device* d3d11Device, const CaptureConfig& config);
    ~DXGICapture() override;

    void start() override;
    void stop() override;
    void setCallback(FrameCallback cb) override;

private:
    void captureLoop(std::stop_token stopToken);
    bool initDuplication();
    void releaseDuplication();

    ID3D11Device* d3d11Device_ = nullptr;
    CaptureConfig config_;
    HMONITOR targetMonitor_ = nullptr;
    FrameCallback callback_;
    std::mutex callbackMutex_;

    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> duplication_;
    std::jthread captureThread_;
    std::atomic<bool> isRunning_{ false };
};

} // namespace LightRec::Capture
