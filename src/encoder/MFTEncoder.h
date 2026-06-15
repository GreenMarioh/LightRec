#pragma once
#include "IEncoder.h"
#include <mfapi.h>
#include <mftransform.h>
#include <wrl/client.h>
#include <vector>
#include <memory>
#include <windows.h>

class MFTEncoder : public IEncoder {
private:
    Microsoft::WRL::ComPtr<IMFTransform> transform_;
    Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> dxgiMgr_;
    UINT dxgiResetToken_ = 0;
    OutputCallback callback_;
    bool initialized_ = false;

    Config config_;
    int64_t lastPts_ = -1;

    void cleanup();
    void processOutput();

public:
    MFTEncoder() = default;
    ~MFTEncoder() override;

    void init(D3D11Device& device, const Config& config) override;
    void encode(ID3D11Texture2D* texture, int64_t pts) override;
    void flush() override;
    void setOutputCallback(OutputCallback cb) override;
};
