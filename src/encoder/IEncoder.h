#pragma once
#include <functional>
#include <vector>
#include <cstdint>
#include <cstring>
#include <d3d11.h>
#include "../core/D3D11Device.h"
#include "../config/Config.h"
#include "../clip/EncodedPacket.h"

using EncodedPacket = lightrec::clip::EncodedPacket;

class IEncoder {
public:
    using OutputCallback = std::function<void(EncodedPacket)>;

    virtual ~IEncoder() = default;
    virtual void init(D3D11Device& device, const Config& config) = 0;
    virtual void encode(ID3D11Texture2D* texture, int64_t pts) = 0;
    virtual void flush() = 0;
    virtual void setOutputCallback(OutputCallback cb) = 0;

    struct NalUnit {
        const uint8_t* data;
        size_t size;
        uint8_t type;
    };

    static inline std::vector<NalUnit> ParseAnnexB(const uint8_t* data, size_t size) {
        std::vector<NalUnit> nals;
        size_t i = 0;
        while (i + 2 < size) {
            if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
                size_t nalStart = i + 3;
                if (!nals.empty()) {
                    size_t prevEnd = i;
                    while (prevEnd > 0 && data[prevEnd - 1] == 0) {
                        prevEnd--;
                    }
                    nals.back().size = prevEnd - (nals.back().data - data);
                }
                NalUnit nal;
                nal.data = data + nalStart;
                nal.size = 0;
                nal.type = data[nalStart] & 0x1F;
                nals.push_back(nal);
                i += 3;
            } else if (i + 3 < size && data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 && data[i + 3] == 1) {
                size_t nalStart = i + 4;
                if (!nals.empty()) {
                    size_t prevEnd = i;
                    while (prevEnd > 0 && data[prevEnd - 1] == 0) {
                        prevEnd--;
                    }
                    nals.back().size = prevEnd - (nals.back().data - data);
                }
                NalUnit nal;
                nal.data = data + nalStart;
                nal.size = 0;
                nal.type = data[nalStart] & 0x1F;
                nals.push_back(nal);
                i += 4;
            } else {
                i++;
            }
        }

        if (!nals.empty()) {
            size_t prevEnd = size;
            while (prevEnd > 0 && data[prevEnd - 1] == 0) {
                prevEnd--;
            }
            nals.back().size = prevEnd - (nals.back().data - data);
        }

        return nals;
    }

    static inline EncodedPacket ConvertAnnexBToAvcc(const uint8_t* data, size_t size, int64_t pts, int64_t duration) {
        auto nals = ParseAnnexB(data, size);

        EncodedPacket packet;
        packet.pts = pts;
        packet.duration = duration;
        packet.isIDR = false;

        size_t avccSize = 0;
        for (const auto& nal : nals) {
            if (nal.size == 0) continue;
            if (nal.type == 7) {
                packet.sps.assign(nal.data, nal.data + nal.size);
            } else if (nal.type == 8) {
                packet.pps.assign(nal.data, nal.data + nal.size);
            } else {
                if (nal.type == 5) {
                    packet.isIDR = true;
                }
                avccSize += 4 + nal.size;
            }
        }

        packet.data.resize(avccSize);
        uint8_t* dst = packet.data.data();
        size_t offset = 0;
        for (const auto& nal : nals) {
            if (nal.size == 0) continue;
            if (nal.type != 7 && nal.type != 8) {
                dst[offset] = (nal.size >> 24) & 0xFF;
                dst[offset + 1] = (nal.size >> 16) & 0xFF;
                dst[offset + 2] = (nal.size >> 8) & 0xFF;
                dst[offset + 3] = nal.size & 0xFF;

                std::memcpy(dst + offset + 4, nal.data, nal.size);
                offset += 4 + nal.size;
            }
        }

        return packet;
    }
};
