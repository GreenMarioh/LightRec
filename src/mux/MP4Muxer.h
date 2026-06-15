#pragma once

#include "IMuxer.h"
#include <fstream>
#include <vector>
#include <string>
#include <mutex>
#include <memory>

namespace lightrec::mux {

class MP4Muxer : public IMuxer {
public:
    struct Config {
        int width = 1920;
        int height = 1080;
        int videoFps = 60;
        
        AudioFormat audioFormat = AudioFormat::AAC;
        int audioSampleRate = 48000;
        int audioChannels = 2;
    };

    MP4Muxer(const Config& config);
    ~MP4Muxer() override;

    void addVideoPacket(const clip::EncodedPacket& packet) override;
    void addAudioSamples(const uint8_t* data, size_t size, int64_t pts, int64_t duration) override;
    bool finalize(const std::filesystem::path& path) override;

private:
    struct SampleInfo {
        size_t size = 0;
        int64_t pts = 0;
        int64_t duration = 0;
        bool isKeyframe = false;
        std::vector<uint8_t> data; // Temporarily hold data until fragment flush
    };

    struct TrackFragment {
        uint32_t trackId = 0;
        int64_t baseDecodeTime = 0;
        std::vector<SampleInfo> samples;
        size_t mdatOffset = 0; // offset in the final mdat for this track
    };

    void flushFragment();
    void writeFtyp();
    void writeMoov();
    void writeMoofAndMdat();

    Config m_config;
    std::ofstream m_file;
    std::mutex m_mutex;

    bool m_headerWritten = false;
    uint32_t m_sequenceNumber = 1;

    std::vector<uint8_t> m_sps;
    std::vector<uint8_t> m_pps;

    TrackFragment m_videoFrag;
    TrackFragment m_audioFrag;

    int64_t m_currentVideoDts = 0;
    int64_t m_currentAudioDts = 0;
};

} // namespace lightrec::mux
