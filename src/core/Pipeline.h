#pragma once

#include <memory>
#include <chrono>
#include <atomic>

#include "../capture/IFrameSource.h"
#include "../encoder/IEncoder.h"
#include "../audio/IAudioSource.h"
#include "../clip/RingBuffer.h"
#include "../audio/AudioRingBuffer.h"
#include "GpuConverter.h"
#include "TexturePool.h"

namespace lightrec::export_ { class Exporter; }

namespace lightrec::core {

class Pipeline {
public:
    Pipeline(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
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
    void saveClip(); // Added for Application

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

    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    std::unique_ptr<GpuConverter> converter_;
    std::unique_ptr<TexturePool> texturePool_;
    uint32_t currentWidth_ = 0;
    uint32_t currentHeight_ = 0;

    std::unique_ptr<lightrec::export_::Exporter> exporter_;
    std::chrono::seconds clipDuration_;

    std::atomic<uint64_t> latest_audio_qpc_{0};
};

} // namespace lightrec::core
