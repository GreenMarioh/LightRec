#pragma once

#include <d3d11.h>
#include <functional>
#include <cstdint>

namespace LightRec::Capture {

using FrameCallback = std::function<void(ID3D11Texture2D* texture, int64_t pts)>;

class IFrameSource {
public:
    virtual ~IFrameSource() = default;

    // Starts the capture stream.
    virtual void start() = 0;

    // Stops the capture stream.
    virtual void stop() = 0;

    // Sets the callback invoked when a new frame is captured.
    virtual void setCallback(FrameCallback cb) = 0;
};

} // namespace LightRec::Capture
