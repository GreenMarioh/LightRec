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

// Manually define Direct3D11 WinRT interop interface
#ifndef __IDirect3DDxgiInterfaceAccess_FWD_DEFINED__
#define __IDirect3DDxgiInterfaceAccess_FWD_DEFINED__
struct IDirect3DDxgiInterfaceAccess : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetInterface(
        REFIID iid,
        void** p
    ) = 0;
};
#endif

// Specialize winrt::impl::guid_v for IDirect3DDxgiInterfaceAccess so that C++/WinRT's .as<>() works under GCC and MSVC
namespace winrt::impl {
    template <>
    inline constexpr guid guid_v<IDirect3DDxgiInterfaceAccess>{ 0xA9B3D0F2, 0x60D1, 0x4B8A, { 0x89, 0x1F, 0xDB, 0x26, 0x30, 0x00, 0x11, 0xD3 } };
}

extern "C" HRESULT WINAPI CreateDirect3D11DeviceFromDXGIDevice(
    IDXGIDevice* dxgiDevice,
    IInspectable** graphicsDevice
);

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
    ThrowIfFailed(interop->CreateForWindow(hwnd, winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(), winrt::put_abi(item)), "Failed to create GraphicsCaptureItem for HWND");
    return item;
}

inline winrt::Windows::Graphics::Capture::GraphicsCaptureItem CreateCaptureItemForMonitor(HMONITOR hmonitor) {
    auto interop = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{ nullptr };
    ThrowIfFailed(interop->CreateForMonitor(hmonitor, winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(), winrt::put_abi(item)), "Failed to create GraphicsCaptureItem for HMONITOR");
    return item;
}

} // namespace LightRec::Capture
