#include "GpuConverter.h"
#include "../util/HRCheck.h"
#include <stdexcept>
#include <fstream>
#include <vector>

namespace lightrec::core {

GpuConverter::GpuConverter(ID3D11Device* device, const std::wstring& shaderPath) {
    using namespace LightRec::Util;

    std::ifstream file(shaderPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("GpuConverter: Failed to open shader file");
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        throw std::runtime_error("GpuConverter: Failed to read shader file");
    }

    ThrowIfFailed(device->CreateComputeShader(buffer.data(), size, nullptr, &computeShader_),
        "Failed to create compute shader for GpuConverter");

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(Params);
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    ThrowIfFailed(device->CreateBuffer(&cbDesc, nullptr, &constantBuffer_),
        "Failed to create constant buffer for GpuConverter");
}

void GpuConverter::convert(ID3D11Texture2D* src, ID3D11Texture2D* dst, ID3D11DeviceContext* ctx) {
    using namespace LightRec::Util;

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    ctx->GetDevice(&device);

    D3D11_TEXTURE2D_DESC srcDesc;
    src->GetDesc(&srcDesc);

    // Create SRV for source
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = srcDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    ThrowIfFailed(device->CreateShaderResourceView(src, &srvDesc, &srv), "Failed to create SRV");

    // Create UAV for destination (NV12 requires specific formats for luma and chroma)
    // NV12 Luma (Y plane) UAV
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavYDesc = {};
    uavYDesc.Format = DXGI_FORMAT_R8_UINT;
    uavYDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;

    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uavY;
    ThrowIfFailed(device->CreateUnorderedAccessView(dst, &uavYDesc, &uavY), "Failed to create UAV Y");

    // NV12 Chroma (UV plane) UAV
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavUVDesc = {};
    uavUVDesc.Format = DXGI_FORMAT_R8G8_UINT;
    uavUVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;

    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uavUV;
    ThrowIfFailed(device->CreateUnorderedAccessView(dst, &uavUVDesc, &uavUV), "Failed to create UAV UV");

    // Update constant buffer
    Params params = { srcDesc.Width, srcDesc.Height, {0, 0} };
    ctx->UpdateSubresource(constantBuffer_.Get(), 0, nullptr, &params, 0, 0);

    // Bind and Dispatch
    ctx->CSSetShader(computeShader_.Get(), nullptr, 0);
    
    ID3D11ShaderResourceView* srvs[] = { srv.Get() };
    ctx->CSSetShaderResources(0, 1, srvs);

    ID3D11UnorderedAccessView* uavs[] = { uavY.Get(), uavUV.Get() };
    ctx->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);

    ID3D11Buffer* cbs[] = { constantBuffer_.Get() };
    ctx->CSSetConstantBuffers(0, 1, cbs);

    // Group size is 16x16
    uint32_t dispatchX = (srcDesc.Width + 15) / 16;
    uint32_t dispatchY = (srcDesc.Height + 15) / 16;
    ctx->Dispatch(dispatchX, dispatchY, 1);

    // Unbind
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr };
    ctx->CSSetShaderResources(0, 1, nullSRVs);

    ID3D11UnorderedAccessView* nullUAVs[] = { nullptr, nullptr };
    ctx->CSSetUnorderedAccessViews(0, 2, nullUAVs, nullptr);

    ctx->CSSetShader(nullptr, nullptr, 0);
}

} // namespace lightrec::core
