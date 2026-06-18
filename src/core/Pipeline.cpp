#include "Pipeline.h"
#include "../export/Exporter.h"

namespace lightrec::core {

Pipeline::Pipeline(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    std::unique_ptr<LightRec::Capture::IFrameSource> frameSource,
    std::unique_ptr<IEncoder> encoder,
    std::unique_ptr<lightrec::audio::IAudioSource> audioSource,
    std::chrono::seconds clipDuration,
    size_t videoBufferSize,
    size_t audioBufferSize
)
    : device_(device)
    , context_(context)
    , frameSource_(std::move(frameSource))
    , encoder_(std::move(encoder))
    , audioSource_(std::move(audioSource))
    , clipDuration_(clipDuration)
    , videoBuffer_(clipDuration, videoBufferSize)
    , audioBuffer_(audioBufferSize)
{
    exporter_ = std::make_unique<lightrec::export_::Exporter>(*this, clipDuration);

    if (frameSource_ && encoder_) {
        frameSource_->setCallback([this](ID3D11Texture2D* texture, int64_t pts) {
            D3D11_TEXTURE2D_DESC desc;
            texture->GetDesc(&desc);

            if (!converter_ || !texturePool_ || currentWidth_ != desc.Width || currentHeight_ != desc.Height) {
                currentWidth_ = desc.Width;
                currentHeight_ = desc.Height;
                converter_ = std::make_unique<GpuConverter>(device_);
                texturePool_ = std::make_unique<TexturePool>(device_, currentWidth_, currentHeight_, DXGI_FORMAT_NV12, 8);
            }

            auto pooledTex = texturePool_->acquire();
            if (pooledTex) {
                converter_->convert(texture, pooledTex->texture, context_);
                encoder_->encode(pooledTex->texture, pts);
                texturePool_->release(pooledTex->slot);
            }
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
    if (exporter_) {
        exporter_->start();
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
    if (exporter_) {
        exporter_->stop();
    }
}

void Pipeline::saveClip() {
    if (exporter_) {
        // Run export asynchronously to not block the caller
        std::thread([this]() {
            exporter_->exportClip(clipDuration_, "LightRecClip");
        }).detach();
    }
}

} // namespace lightrec::core
