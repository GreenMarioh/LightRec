#pragma once
#include <d3d11.h>
#include <dxgi1_3.h>
#include <wrl/client.h>

class D3D11Device {
public:
    D3D11Device();
    ~D3D11Device() = default;

    // Disallow copy/move since it holds device state (optional but good practice)
    D3D11Device(const D3D11Device&) = delete;
    D3D11Device& operator=(const D3D11Device&) = delete;

    ID3D11Device* device() const { return device_.Get(); }
    ID3D11DeviceContext* context() const { return context_.Get(); }
    IDXGIAdapter* adapter() const { return adapter_.Get(); }
    IDXGIFactory2* factory() const { return factory_.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_;
    Microsoft::WRL::ComPtr<IDXGIFactory2> factory_;
};
