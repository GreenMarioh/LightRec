#include "EncoderFactory.h"
#include "QSVEncoder.h"
#include "NVENCEncoder.h"
#include "AMFEncoder.h"
#include "MFTEncoder.h"
#include <stdexcept>

GpuVendor EncoderFactory::probeGpuVendor(IDXGIAdapter* adapter) {
    if (!adapter) {
        return GpuVendor::Unknown;
    }
    DXGI_ADAPTER_DESC desc{};
    if (SUCCEEDED(adapter->GetDesc(&desc))) {
        if (desc.VendorId == static_cast<UINT>(GpuVendor::Intel)) return GpuVendor::Intel;
        if (desc.VendorId == static_cast<UINT>(GpuVendor::Nvidia)) return GpuVendor::Nvidia;
        if (desc.VendorId == static_cast<UINT>(GpuVendor::Amd)) return GpuVendor::Amd;
    }
    return GpuVendor::Unknown;
}

std::unique_ptr<IEncoder> EncoderFactory::create(D3D11Device& device, const Config& config) {
    GpuVendor vendor = probeGpuVendor(device.adapter());

    // First try the vendor-specific hardware encoder
    if (vendor == GpuVendor::Intel) {
        try {
            auto enc = std::make_unique<QSVEncoder>();
            enc->init(device, config);
            return enc;
        } catch (...) {
            // Fall through to try others
        }
    } else if (vendor == GpuVendor::Nvidia) {
        try {
            auto enc = std::make_unique<NVENCEncoder>();
            enc->init(device, config);
            return enc;
        } catch (...) {
            // Fall through to try others
        }
    } else if (vendor == GpuVendor::Amd) {
        try {
            auto enc = std::make_unique<AMFEncoder>();
            enc->init(device, config);
            return enc;
        } catch (...) {
            // Fall through to try others
        }
    }

    // Fallback: try all hardware encoders in standard priority order:
    // 1. Intel QSV
    if (vendor != GpuVendor::Intel) {
        try {
            auto enc = std::make_unique<QSVEncoder>();
            enc->init(device, config);
            return enc;
        } catch (...) {}
    }

    // 2. NVIDIA NVENC
    if (vendor != GpuVendor::Nvidia) {
        try {
            auto enc = std::make_unique<NVENCEncoder>();
            enc->init(device, config);
            return enc;
        } catch (...) {}
    }

    // 3. AMD AMF
    if (vendor != GpuVendor::Amd) {
        try {
            auto enc = std::make_unique<AMFEncoder>();
            enc->init(device, config);
            return enc;
        } catch (...) {}
    }

    // 4. Windows Media Foundation Transform (MFT) fallback
    try {
        auto enc = std::make_unique<MFTEncoder>();
        enc->init(device, config);
        return enc;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("EncoderFactory: Failed to initialize any encoder. MFT fallback failed: ") + e.what());
    }
}
