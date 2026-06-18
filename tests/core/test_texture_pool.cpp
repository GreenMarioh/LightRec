#include "../../src/core/TexturePool.h"
#include <iostream>
#include <cassert>
#include <d3d11.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d11.lib")

using lightrec::core::TexturePool;
using Microsoft::WRL::ComPtr;

int main() {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context);
    
    if (FAILED(hr)) {
        std::cout << "Failed to create D3D11 device, skipping test\n";
        return 0;
    }
    
    uint32_t width = 1920;
    uint32_t height = 1080;
    DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;
    uint32_t count = 4;
    
    TexturePool pool(device.Get(), width, height, format, count);
    
    std::vector<lightrec::core::PooledTexture> textures;
    for (uint32_t i = 0; i < count; ++i) {
        auto t = pool.acquire();
        if (t) {
            textures.push_back(*t);
        }
    }
    
    assert(textures.size() == count);
    
    for (const auto& t : textures) {
        assert(t.texture != nullptr);
        assert(t.texture == pool.texture(t.slot));
    }
    
    auto empty_t = pool.acquire();
    assert(!empty_t.has_value());
    
    pool.release(textures[0].slot);
    auto t_reacquired = pool.acquire();
    assert(t_reacquired.has_value());
    assert(t_reacquired->slot == textures[0].slot);
    
    std::cout << "TexturePool tests passed\n";
    return 0;
}
