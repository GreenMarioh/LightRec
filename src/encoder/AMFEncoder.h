#pragma once
#include "IEncoder.h"
#include <core/Factory.h>
#include <core/Context.h>
#include <components/VideoEncoderVCE.h>
#include <wrl/client.h>
#include <vector>
#include <memory>
#include <windows.h>

class AMFEncoder : public IEncoder {
private:
    HMODULE amfLib_ = nullptr;
    amf::AMFFactory* factory_ = nullptr;
    amf::AMFContextPtr context_;
    amf::AMFComponentPtr encoder_;
    OutputCallback callback_;
    bool initialized_ = false;

    Config config_;
    int64_t lastPts_ = -1;

    void cleanup();
    void pollOutput();

public:
    AMFEncoder() = default;
    ~AMFEncoder() override;

    void init(D3D11Device& device, const Config& config) override;
    void encode(ID3D11Texture2D* texture, int64_t pts) override;
    void flush() override;
    void setOutputCallback(OutputCallback cb) override;
};
