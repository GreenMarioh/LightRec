#include "../../src/mux/MP4Muxer.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace lightrec::mux;
using namespace lightrec::clip;

TEST(MP4MuxerTest, BasicFinalizeWithoutHeader) {
    MP4Muxer::Config config;
    MP4Muxer muxer(config);
    
    std::filesystem::path outPath = std::filesystem::temp_directory_path() / "test_mux1.mp4";
    // Fails because no header/sps was ever given
    EXPECT_FALSE(muxer.finalize(outPath));
}

TEST(MP4MuxerTest, WriteKeyframeAndFinalize) {
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

    std::filesystem::path outPath = std::filesystem::temp_directory_path() / "test_mux2.mp4";
    EXPECT_TRUE(muxer.finalize(outPath));

    EXPECT_TRUE(std::filesystem::exists(outPath));
    EXPECT_GT(std::filesystem::file_size(outPath), 500); 
    
    std::filesystem::remove(outPath);
}

TEST(MP4MuxerTest, WriteMultipleFragments) {
    MP4Muxer::Config config;
    MP4Muxer muxer(config);
    
    std::filesystem::path outPath = std::filesystem::temp_directory_path() / "test_mux3.mp4";

    // 1st GOP
    {
        EncodedPacket packet;
        packet.isIDR = true;
        packet.pts = 0;
        packet.duration = 166666;
        packet.sps = {0x67, 0x42}; packet.pps = {0x68, 0xCE};
        packet.data = {0x00, 0x00, 0x00, 0x01, 0x65, 0x01};
        muxer.addVideoPacket(packet);
        
        packet.isIDR = false;
        packet.pts = 166666;
        packet.data = {0x00, 0x00, 0x00, 0x01, 0x41, 0x02};
        muxer.addVideoPacket(packet);
    }
    
    // 2nd GOP (forces flush of first)
    {
        EncodedPacket packet;
        packet.isIDR = true;
        packet.pts = 333332;
        packet.duration = 166666;
        packet.sps = {0x67, 0x42}; packet.pps = {0x68, 0xCE};
        packet.data = {0x00, 0x00, 0x00, 0x01, 0x65, 0x03};
        muxer.addVideoPacket(packet);
    }

    EXPECT_TRUE(muxer.finalize(outPath));
    EXPECT_GT(std::filesystem::file_size(outPath), 1000);
    
    std::filesystem::remove(outPath);
}
