#pragma once
#include "IEncoder.h"
#include <ffnvcodec/nvEncodeAPI.h>
#include <wrl/client.h>
#include <vector>
#include <unordered_map>
#include <windows.h>

class NVENCEncoder : public IEncoder {
private:
    HMODULE nvencLib_ = nullptr;
    NV_ENCODE_API_FUNCTION_LIST nvencApi_{};
    void* encoder_ = nullptr;
    OutputCallback callback_;
    bool initialized_ = false;

    // Direct3D11 device
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11Device_;

    // Config
    Config config_;

    // Resource registration map to prevent duplicate registration
    std::unordered_map<ID3D11Texture2D*, NV_ENC_REGISTERED_RESOURCE> registeredResources_;

    // Asynchronous output buffers
    struct OutputBuffer {
        NV_ENC_OUTPUT_PTR bitstream = nullptr;
        HANDLE event = nullptr;
        bool busy = false;
        int64_t pts = 0;
        int64_t duration = 0;
    };
    std::vector<OutputBuffer> outputBuffers_;
    size_t currentBufferIdx_ = 0;

    int64_t lastPts_ = -1;

    void cleanup();
    void processOutputBuffer(OutputBuffer& buf);

public:
    NVENCEncoder() = default;
    ~NVENCEncoder() override;

    void init(D3D11Device& device, const Config& config) override;
    void encode(ID3D11Texture2D* texture, int64_t pts) override;
    void flush() override;
    void setOutputCallback(OutputCallback cb) override;
};
