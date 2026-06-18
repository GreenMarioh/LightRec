#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <string>

namespace lightrec::core {

class GpuConverter {
public:
    explicit GpuConverter(ID3D11Device* device, const std::wstring& shaderPath = L"shaders/RgbToNv12.cso");
    ~GpuConverter() = default;

    GpuConverter(const GpuConverter&) = delete;
    GpuConverter& operator=(const GpuConverter&) = delete;

    void convert(ID3D11Texture2D* src, ID3D11Texture2D* dst, ID3D11DeviceContext* ctx);

private:
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> computeShader_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer_;
    
    struct Params {
        uint32_t width;
        uint32_t height;
        uint32_t pad[2];
    };
};

} // namespace lightrec::core
