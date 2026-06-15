#pragma once
#include "IEncoder.h"
#include <vpl/mfx.h>
#include <wrl/client.h>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>

class QSVEncoder : public IEncoder {
private:
    mfxLoader loader_ = nullptr;
    mfxSession session_ = nullptr;
    OutputCallback callback_;
    bool initialized_ = false;

    // Encoder configuration
    mfxVideoParam encodeParams_{};
    mfxFrameAllocRequest allocRequest_{};
    std::vector<uint8_t> bitstreamBuffer_;
    mfxBitstream bitstream_{};

    // Keep track of timestamps for duration calculation
    int64_t lastPts_ = -1;

    void cleanup();

public:
    QSVEncoder() = default;
    ~QSVEncoder() override;

    void init(D3D11Device& device, const Config& config) override;
    void encode(ID3D11Texture2D* texture, int64_t pts) override;
    void flush() override;
    void setOutputCallback(OutputCallback cb) override;
};
