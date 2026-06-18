#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <optional>
#include <vector>
#include "../util/SPSCQueue.h"

namespace lightrec::core {

struct PooledTexture {
    ID3D11Texture2D* texture;
    uint32_t slot;
};

class TexturePool {
public:
    TexturePool(ID3D11Device* device, uint32_t width, uint32_t height, DXGI_FORMAT format, uint32_t count);
    ~TexturePool() = default;

    TexturePool(const TexturePool&) = delete;
    TexturePool& operator=(const TexturePool&) = delete;

    std::optional<PooledTexture> acquire();
    void release(uint32_t slot);
    ID3D11Texture2D* texture(uint32_t slot);

private:
    std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>> textures_;
    LightRec::Util::SPSCQueue<uint32_t> freeList_;
};

} // namespace lightrec::core
