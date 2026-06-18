#pragma once

#include <vector>
#include <span>
#include <cstdint>
#include <windows.h>
#include <wrl/client.h>
#include <mfapi.h>
#include <mftransform.h>

namespace lightrec::audio {

class AudioEncoder {
public:
    AudioEncoder(uint32_t sampleRate, uint32_t channels, uint32_t bitrate = 192000);
    ~AudioEncoder();

    // Non-copyable, non-movable
    AudioEncoder(const AudioEncoder&) = delete;
    AudioEncoder& operator=(const AudioEncoder&) = delete;
    AudioEncoder(AudioEncoder&&) = delete;
    AudioEncoder& operator=(AudioEncoder&&) = delete;

    /**
     * @brief Feeds float PCM to the AAC encoder and returns any encoded AAC frames.
     * The input float PCM is expected to be interleaved if channels > 1.
     * 
     * @param pcmSamples Interleaved PCM float samples.
     * @return Encoded AAC frames.
     */
    std::vector<uint8_t> encode(std::span<const float> pcmSamples);

    /**
     * @brief Drains remaining samples from the encoder.
     * 
     * @return Encoded AAC frames from the drain process.
     */
    std::vector<uint8_t> flush();

    /**
     * @brief Returns the AAC AudioSpecificConfig blob required for MP4 muxing.
     * 
     * @return AudioSpecificConfig data.
     */
    std::vector<uint8_t> getAudioSpecificConfig() const;

private:
    void initMFT();
    void setOutputType();
    void setInputType();
    std::vector<uint8_t> processOutput();
    void checkHResult(HRESULT hr, const char* msg);

    uint32_t sampleRate_;
    uint32_t channels_;
    uint32_t bitrate_;

    Microsoft::WRL::ComPtr<IMFTransform> mft_;
    DWORD inputStreamId_ = 0;
    DWORD outputStreamId_ = 0;

    std::vector<int16_t> pcmConversionBuffer_;
    uint64_t inputSampleTime_ = 0;
};

} // namespace lightrec::audio
