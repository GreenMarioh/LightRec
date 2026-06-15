#include "../../src/mux/MP4Muxer.h"
#include <iostream>
#include <filesystem>
#include <vector>

using namespace lightrec::mux;
using namespace lightrec::clip;

void testMuxer() {
    MP4Muxer::Config config;
    config.width = 1920;
    config.height = 1080;
    config.audioFormat = AudioFormat::AAC;
    config.audioSampleRate = 48000;
    config.audioChannels = 2;

    MP4Muxer muxer(config);
    
    EncodedPacket packet;
    packet.isIDR = true;
    packet.pts = 0;
    packet.duration = 166666; // 60fps approx
    packet.sps = {0x67, 0x42, 0x00, 0x1F, 0xE5}; 
    packet.pps = {0x68, 0xCE, 0x3C, 0x80}; 
    packet.data = {0x00, 0x00, 0x00, 0x01, 0x65, 0x01, 0x02, 0x03};

    muxer.addVideoPacket(packet);

    std::vector<uint8_t> audioData(256, 0); 
    muxer.addAudioSamples(audioData.data(), audioData.size(), 0, 1024LL * 10000000 / 48000);

    std::filesystem::path outPath = "test_standalone.mp4";
    if (muxer.finalize(outPath)) {
        std::cout << "Muxing successful! File size: " << std::filesystem::file_size(outPath) << std::endl;
        std::filesystem::remove(outPath);
    } else {
        std::cerr << "Muxing failed!" << std::endl;
    }
}

int main() {
    testMuxer();
    return 0;
}
