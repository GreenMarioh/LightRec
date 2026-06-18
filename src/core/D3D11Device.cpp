#include "D3D11Device.h"
#include "../util/HRCheck.h"
#include <stdexcept>

D3D11Device::D3D11Device() {
    using namespace LightRec::Util;

    ThrowIfFailed(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory_)), "CreateDXGIFactory2 failed");
    
    // Enumerate first adapter
    ThrowIfFailed(factory_->EnumAdapters(0, &adapter_), "EnumAdapters failed (no GPU found?)");

    UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0
    };
    D3D_FEATURE_LEVEL featureLevel;

    ThrowIfFailed(D3D11CreateDevice(
        adapter_.Get(),
        D3D_DRIVER_TYPE_UNKNOWN, // Must be UNKNOWN if adapter is provided
        nullptr,
        createDeviceFlags,
        featureLevels,
        1,
        D3D11_SDK_VERSION,
        &device_,
        &featureLevel,
        &context_
    ), "D3D11CreateDevice failed");
}
