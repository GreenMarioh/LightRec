#pragma once

#include "IFrameSource.h"
#include "CaptureConfig.h"
#include <memory>
#include <d3d11.h>

namespace LightRec::Capture {

class CaptureFactory {
public:
    static std::unique_ptr<IFrameSource> create(ID3D11Device* device, const CaptureConfig& config);
    static bool isWGCAvailable();
};

} // namespace LightRec::Capture
