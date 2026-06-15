#include "DXGICapture.h"
#include <stdexcept>

namespace LightRec::Capture {

DXGICapture::DXGICapture(ID3D11Device* d3d11Device, const CaptureConfig& config)
    : d3d11Device_(d3d11Device)
    , config_(config)
{
    if (!d3d11Device_) {
        throw std::invalid_argument("d3d11Device cannot be nullptr");
    }

    HMONITOR monitor = config_.targetMonitor;
    if (config_.targetType == CaptureTargetType::Window) {
        if (config_.targetWindow) {
            monitor = MonitorFromWindow(config_.targetWindow, MONITOR_DEFAULTTOPRIMARY);
        }
    }

    if (!monitor) {
        monitor = MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    }

    if (!monitor) {
        throw std::runtime_error("Failed to find a valid monitor for DXGI capture");
    }

    targetMonitor_ = monitor;
}

DXGICapture::~DXGICapture() {
    stop();
}

void DXGICapture::start() {
    if (isRunning_.exchange(true)) {
        return;
    }

    captureThread_ = std::jthread([this](std::stop_token stopToken) {
        captureLoop(stopToken);
    });
}

void DXGICapture::stop() {
    if (!isRunning_.exchange(false)) {
        return;
    }

    captureThread_.request_stop();
    if (captureThread_.joinable()) {
        captureThread_.join();
    }
}

void DXGICapture::setCallback(FrameCallback cb) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    callback_ = cb;
}

void DXGICapture::captureLoop(std::stop_token stopToken) {
    bool needsInit = true;

    while (!stopToken.stop_requested()) {
        if (needsInit) {
            try {
                if (initDuplication()) {
                    needsInit = false;
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }
            } catch (...) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
        }

        DXGI_OUTDUPL_FRAME_INFO frameInfo{};
        Microsoft::WRL::ComPtr<IDXGIResource> dxgiResource;

        HRESULT hr = duplication_->AcquireNextFrame(16, &frameInfo, &dxgiResource);

        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            continue;
        }

        if (FAILED(hr)) {
            releaseDuplication();
            needsInit = true;

            if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_DEVICE_REMOVED) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            continue;
        }

        Microsoft::WRL::ComPtr<ID3D11Texture2D> sourceTexture;
        hr = dxgiResource->As(&sourceTexture);
        if (SUCCEEDED(hr) && sourceTexture) {
            FrameCallback cb;
            {
                std::lock_guard<std::mutex> lock(callbackMutex_);
                cb = callback_;
            }

            if (cb) {
                LARGE_INTEGER qpc;
                QueryPerformanceCounter(&qpc);

                LARGE_INTEGER freq;
                QueryPerformanceFrequency(&freq);

                int64_t qpcValue = qpc.QuadPart;
                int64_t qpcFreq = freq.QuadPart;
                int64_t pts = (qpcValue / qpcFreq) * 10'000'000 + ((qpcValue % qpcFreq) * 10'000'000) / qpcFreq;

                try {
                    cb(sourceTexture.Get(), pts);
                } catch (...) {
                    // Suppress callback exceptions
                }
            }
        }

        duplication_->ReleaseFrame();
    }

    releaseDuplication();
}

bool DXGICapture::initDuplication() {
    try {
        Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
        ThrowIfFailed(d3d11Device_->QueryInterface(IID_PPV_ARGS(&dxgiDevice)), "Failed to query IDXGIDevice");

        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
        ThrowIfFailed(dxgiDevice->GetAdapter(&adapter), "Failed to get IDXGIAdapter");

        UINT i = 0;
        Microsoft::WRL::ComPtr<IDXGIOutput> output;
        bool found = false;
        while (adapter->EnumOutputs(i++, &output) != DXGI_ERROR_NOT_FOUND) {
            DXGI_OUTPUT_DESC desc;
            output->GetDesc(&desc);
            if (desc.Monitor == targetMonitor_) {
                found = true;
                break;
            }
            output.Reset();
        }

        if (!found) {
            return false;
        }

        Microsoft::WRL::ComPtr<IDXGIOutput1> output1;
        ThrowIfFailed(output->QueryInterface(IID_PPV_ARGS(&output1)), "Failed to query IDXGIOutput1");

        Microsoft::WRL::ComPtr<IDXGIOutput6> output6;
        HRESULT hr = output1->QueryInterface(IID_PPV_ARGS(&output6));
        if (SUCCEEDED(hr)) {
            DXGI_FORMAT formats[] = { DXGI_FORMAT_B8G8R8A8_UNORM };
            hr = output6->DuplicateOutput1(d3d11Device_, 0, ARRAYSIZE(formats), formats, &duplication_);
        }
        if (FAILED(hr)) {
            hr = output1->DuplicateOutput(d3d11Device_, &duplication_);
        }

        if (FAILED(hr)) {
            return false;
        }

        return true;
    } catch (...) {
        return false;
    }
}

void DXGICapture::releaseDuplication() {
    duplication_.Reset();
}

} // namespace LightRec::Capture
