#include "TexturePool.h"
#include "../util/HRCheck.h"
#include <stdexcept>

namespace lightrec::core {

TexturePool::TexturePool(ID3D11Device* device, uint32_t width, uint32_t height, DXGI_FORMAT format, uint32_t count)
    : freeList_([](uint32_t c) {
        uint32_t cap = 1;
        while (cap <= c) cap <<= 1;
        return cap;
      }(count)) 
{
    using namespace LightRec::Util;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    
    // Add bind flags based on typical uses (NV12 might need both SRV and UAV/RTV depending on the shader)
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    if (format != DXGI_FORMAT_NV12) {
        desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    }
    
    textures_.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        ThrowIfFailed(device->CreateTexture2D(&desc, nullptr, &tex), "Failed to create texture in TexturePool");
        textures_.push_back(tex);
        
        bool pushed = freeList_.try_push(i);
        if (!pushed) {
            throw std::runtime_error("TexturePool failed to initialize free list (capacity too small?)");
        }
    }
}

std::optional<PooledTexture> TexturePool::acquire() {
    uint32_t slot;
    if (freeList_.try_pop(slot)) {
        return PooledTexture{ textures_[slot].Get(), slot };
    }
    return std::nullopt;
}

void TexturePool::release(uint32_t slot) {
    if (slot < textures_.size()) {
        freeList_.try_push(slot);
    }
}

ID3D11Texture2D* TexturePool::texture(uint32_t slot) {
    if (slot < textures_.size()) {
        return textures_[slot].Get();
    }
    return nullptr;
}

} // namespace lightrec::core
