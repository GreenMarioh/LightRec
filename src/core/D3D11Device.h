#pragma once
#include <d3d11.h>
#include <wrl/client.h>

class D3D11Device {
public:
    D3D11Device() = default;
    ~D3D11Device() = default;

    ID3D11Device* getDevice() const { return device_.Get(); }
    ID3D11DeviceContext* getContext() const { return context_.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
};
