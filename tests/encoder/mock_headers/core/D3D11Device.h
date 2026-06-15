#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <dxgi1_2.h>

class D3D11Device {
private:
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_;
public:
    D3D11Device() = default;
    D3D11Device(Microsoft::WRL::ComPtr<ID3D11Device> dev, Microsoft::WRL::ComPtr<ID3D11DeviceContext> ctx, Microsoft::WRL::ComPtr<IDXGIAdapter> adp)
        : device_(dev), context_(ctx), adapter_(adp) {}

    ID3D11Device* device() const { return device_.Get(); }
    ID3D11DeviceContext* context() const { return context_.Get(); }
    IDXGIAdapter* adapter() const { return adapter_.Get(); }
};
