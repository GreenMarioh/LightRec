#pragma once

#include <memory>
#include <chrono>
#include <atomic>

#include "../capture/IFrameSource.h"
#include "../encoder/IEncoder.h"
#include "../audio/IAudioSource.h"
#include "../clip/RingBuffer.h"
#include "../audio/AudioRingBuffer.h"

namespace lightrec::core {

class Pipeline {
public:
    Pipeline(
        std::unique_ptr<LightRec::Capture::IFrameSource> frameSource,
        std::unique_ptr<IEncoder> encoder,
        std::unique_ptr<lightrec::audio::IAudioSource> audioSource,
        std::chrono::seconds clipDuration,
        size_t videoBufferSize,
        size_t audioBufferSize
    );

    ~Pipeline();

    // Prevent copy/move
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    void start();
    void stop();

    lightrec::clip::RingBuffer& getVideoBuffer() { return videoBuffer_; }
    lightrec::audio::AudioRingBuffer& getAudioBuffer() { return audioBuffer_; }
    
    // Shared QPC timeline tracking
    uint64_t getLatestAudioQpc() const { return latest_audio_qpc_.load(std::memory_order_acquire); }

private:
    std::unique_ptr<LightRec::Capture::IFrameSource> frameSource_;
    std::unique_ptr<IEncoder> encoder_;
    std::unique_ptr<lightrec::audio::IAudioSource> audioSource_;

    lightrec::clip::RingBuffer videoBuffer_;
    lightrec::audio::AudioRingBuffer audioBuffer_;

    std::atomic<uint64_t> latest_audio_qpc_{0};
};

} // namespace lightrec::core
