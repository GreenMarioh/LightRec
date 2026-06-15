#include "../../src/core/Pipeline.h"
#include <gtest/gtest.h>

class MockFrameSource : public LightRec::Capture::IFrameSource {
public:
    void start() override {}
    void stop() override {}
    void setCallback(LightRec::Capture::FrameCallback cb) override { callback = cb; }
    
    LightRec::Capture::FrameCallback callback;
};

class MockEncoder : public IEncoder {
public:
    void init(D3D11Device& device, const Config& config) override {}
    void encode(ID3D11Texture2D* texture, int64_t pts) override {
        if (callback) {
            lightrec::clip::EncodedPacket p;
            p.pts = pts;
            callback(p);
        }
    }
    void flush() override {}
    void setOutputCallback(OutputCallback cb) override { callback = cb; }
    
    OutputCallback callback;
};

class MockAudioSource : public lightrec::audio::IAudioSource {
public:
    void start() override {}
    void stop() override {}
    void setCallback(AudioCallback cb) override { callback = cb; }
    void setVolume(lightrec::audio::AudioDeviceType type, float volume) override {}
    float getVolume(lightrec::audio::AudioDeviceType type) const override { return 1.0f; }
    void setMicrophoneEnabled(bool enabled) override {}
    bool isMicrophoneEnabled() const override { return false; }
    
    AudioCallback callback;
};

TEST(PipelineTest, BasicIntegration) {
    auto frameSource = std::make_unique<MockFrameSource>();
    auto encoder = std::make_unique<MockEncoder>();
    auto audioSource = std::make_unique<MockAudioSource>();
    
    auto* fsPtr = frameSource.get();
    auto* encPtr = encoder.get();
    auto* asPtr = audioSource.get();

    lightrec::core::Pipeline pipeline(
        std::move(frameSource),
        std::move(encoder),
        std::move(audioSource),
        std::chrono::seconds(15),
        1024 * 1024,
        1024
    );

    pipeline.start();

    // Simulate video frame
    if (fsPtr->callback) {
        fsPtr->callback(nullptr, 12345);
    }
    
    // Simulate audio
    if (asPtr->callback) {
        std::vector<float> audioData(100, 0.5f);
        asPtr->callback(audioData.data(), audioData.size(), 9999);
    }

    pipeline.stop();

    auto videoPackets = pipeline.getVideoBuffer().snapshot();
    EXPECT_EQ(videoPackets.size(), 1);
    if (!videoPackets.empty()) {
        EXPECT_EQ(videoPackets[0].pts, 12345);
    }

    EXPECT_EQ(pipeline.getAudioBuffer().size(), 100);
    EXPECT_EQ(pipeline.getLatestAudioQpc(), 9999);
}
