#pragma once
#include <vector>
#include <cstdint>

namespace lightrec::clip {

struct EncodedPacket {
    std::vector<uint8_t> data;
    int64_t pts = 0;          // Presentation timestamp in 100ns units
    int64_t duration = 0;     // Frame duration in 100ns units
    bool isIDR = false;       // Keyframe flag
    std::vector<uint8_t> sps; // SPS NAL (present only on IDR)
    std::vector<uint8_t> pps; // PPS NAL (present only on IDR)
};

} // namespace lightrec::clip
