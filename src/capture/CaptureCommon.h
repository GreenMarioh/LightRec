#pragma once

#include <unknwn.h>
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <system_error>
#include <string>
#include <iostream>

// C++/WinRT headers
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Foundation.Metadata.h>

#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

namespace LightRec::Capture {

inline void ThrowIfFailed(HRESULT hr, const std::string& message) {
    if (FAILED(hr)) {
        throw std::system_error(hr, std::system_category(), message);
    }
}

inline winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice CreateDirect3DDevice(ID3D11Device* d3d11Device) {
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    ThrowIfFailed(d3d11Device->QueryInterface(IID_PPV_ARGS(&dxgiDevice)), "Failed to query IDXGIDevice from ID3D11Device");

    winrt::com_ptr<IInspectable> inspectable;
    ThrowIfFailed(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), inspectable.put()), "Failed to create WinRT Direct3D11 device from DXGI device");

    return inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
}

inline winrt::Windows::Graphics::Capture::GraphicsCaptureItem CreateCaptureItemForWindow(HWND hwnd) {
    auto interop = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{ nullptr };
    ThrowIfFailed(interop->CreateForWindow(hwnd, winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(), winrt::put_void(item)), "Failed to create GraphicsCaptureItem for HWND");
    return item;
}

inline winrt::Windows::Graphics::Capture::GraphicsCaptureItem CreateCaptureItemForMonitor(HMONITOR hmonitor) {
    auto interop = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{ nullptr };
    ThrowIfFailed(interop->CreateForMonitor(hmonitor, winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(), winrt::put_void(item)), "Failed to create GraphicsCaptureItem for HMONITOR");
    return item;
}

} // namespace LightRec::Capture
