#include "Pipeline.h"

namespace lightrec::core {

Pipeline::Pipeline(
    std::unique_ptr<LightRec::Capture::IFrameSource> frameSource,
    std::unique_ptr<IEncoder> encoder,
    std::unique_ptr<lightrec::audio::IAudioSource> audioSource,
    std::chrono::seconds clipDuration,
    size_t videoBufferSize,
    size_t audioBufferSize
)
    : frameSource_(std::move(frameSource))
    , encoder_(std::move(encoder))
    , audioSource_(std::move(audioSource))
    , videoBuffer_(clipDuration, videoBufferSize)
    , audioBuffer_(audioBufferSize)
{
    if (frameSource_ && encoder_) {
        frameSource_->setCallback([this](ID3D11Texture2D* texture, int64_t pts) {
            encoder_->encode(texture, pts);
        });

        encoder_->setOutputCallback([this](lightrec::clip::EncodedPacket packet) {
            videoBuffer_.push(packet);
        });
    }

    if (audioSource_) {
        audioSource_->setCallback([this](const float* data, size_t sample_count, uint64_t timestamp_qpc) {
            audioBuffer_.write(data, sample_count);
            latest_audio_qpc_.store(timestamp_qpc, std::memory_order_release);
        });
    }
}

Pipeline::~Pipeline() {
    stop();
}

void Pipeline::start() {
    if (audioSource_) {
        audioSource_->start();
    }
    if (frameSource_) {
        frameSource_->start();
    }
}

void Pipeline::stop() {
    if (frameSource_) {
        frameSource_->stop();
    }
    if (audioSource_) {
        audioSource_->stop();
    }
    if (encoder_) {
        encoder_->flush();
    }
}

} // namespace lightrec::core
