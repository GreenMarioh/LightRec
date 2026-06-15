#include "CaptureFactory.h"
#include "WGCCapture.h"
#include "DXGICapture.h"

namespace LightRec::Capture {

std::unique_ptr<IFrameSource> CaptureFactory::create(ID3D11Device* device, const CaptureConfig& config) {
    if (isWGCAvailable()) {
        try {
            return std::make_unique<WGCCapture>(device, config);
        } catch (...) {
            // Fall back to DXGI if WGC initialization fails (e.g., in virtualized/headless environments)
        }
    }
    return std::make_unique<DXGICapture>(device, config);
}

bool CaptureFactory::isWGCAvailable() {
    try {
        bool isTypePresent = winrt::Windows::Foundation::Metadata::ApiInformation::IsTypePresent(
            L"Windows.Graphics.Capture.GraphicsCaptureItem");
        if (!isTypePresent) {
            return false;
        }
        return winrt::Windows::Graphics::Capture::GraphicsCaptureSession::IsSupported();
    } catch (...) {
        return false;
    }
}

} // namespace LightRec::Capture
