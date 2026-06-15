#pragma once
#include <vector>
#include <cstdint>

struct EncodedPacket {
    std::vector<uint8_t> data;        // H.264 NAL unit(s), AVCC format
    int64_t pts = 0;                  // presentation time (100ns units)
    int64_t duration = 0;             // frame duration (100ns units)
    bool isIDR = false;               // keyframe flag
    std::vector<uint8_t> sps;         // SPS NAL (present only on IDR)
    std::vector<uint8_t> pps;         // PPS NAL (present only on IDR)
};
