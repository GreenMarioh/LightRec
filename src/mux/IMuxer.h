#pragma once

#include <filesystem>
#include <cstdint>
#include <cstddef>
#include "../clip/EncodedPacket.h"

namespace lightrec::mux {

enum class AudioFormat {
    AAC,
    PCM_Float,
    PCM_16
};

class IMuxer {
public:
    virtual ~IMuxer() = default;

    virtual void addVideoPacket(const clip::EncodedPacket& packet) = 0;
    
    // Duration and pts in 100ns units
    virtual void addAudioSamples(const uint8_t* data, size_t size, int64_t pts, int64_t duration) = 0;
    
    virtual bool finalize(const std::filesystem::path& path) = 0;
};

} // namespace lightrec::mux
