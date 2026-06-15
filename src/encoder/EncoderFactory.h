#pragma once
#include "IEncoder.h"
#include <memory>
#include <dxgi.h>

enum class GpuVendor {
    Intel = 0x8086,
    Nvidia = 0x10DE,
    Amd = 0x1002,
    Unknown = 0
};

class EncoderFactory {
public:
    static std::unique_ptr<IEncoder> create(D3D11Device& device, const Config& config);
    static GpuVendor probeGpuVendor(IDXGIAdapter* adapter);
};
