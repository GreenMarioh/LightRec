#include "AudioEncoder.h"
#include <stdexcept>
#include <string>
#include <algorithm>
#include <mfapi.h>
#include <mferror.h>
#include <wmcodecdsp.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")

namespace lightrec::audio {

AudioEncoder::AudioEncoder(uint32_t sampleRate, uint32_t channels, uint32_t bitrate)
    : sampleRate_(sampleRate), channels_(channels), bitrate_(bitrate) {
    MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    initMFT();
}

AudioEncoder::~AudioEncoder() {
    if (mft_) {
        mft_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
        mft_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
        mft_ = nullptr;
    }
    MFShutdown();
}

void AudioEncoder::checkHResult(HRESULT hr, const char* msg) {
    if (FAILED(hr)) {
        throw std::runtime_error(std::string(msg) + " (HRESULT: " + std::to_string(hr) + ")");
    }
}

void AudioEncoder::initMFT() {
    MFT_REGISTER_TYPE_INFO outInfo = { MFMediaType_Audio, MFAudioFormat_AAC };
    MFT_REGISTER_TYPE_INFO inInfo = { MFMediaType_Audio, MFAudioFormat_PCM };

    IMFActivate** ppActivate = nullptr;
    UINT32 count = 0;

    HRESULT hr = MFTEnumEx(
        MFT_CATEGORY_AUDIO_ENCODER,
        MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT | MFT_ENUM_FLAG_SORTANDFILTER,
        &inInfo,
        &outInfo,
        &ppActivate,
        &count
    );

    checkHResult(hr, "Failed to enumerate audio encoders");

    if (count == 0) {
        throw std::runtime_error("No AAC audio encoder found");
    }

    hr = ppActivate[0]->ActivateObject(IID_PPV_ARGS(&mft_));
    
    for (UINT32 i = 0; i < count; i++) {
        ppActivate[i]->Release();
    }
    CoTaskMemFree(ppActivate);

    checkHResult(hr, "Failed to activate AAC encoder MFT");

    setOutputType();
    setInputType();

    hr = mft_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    checkHResult(hr, "Failed to notify begin streaming");
    
    hr = mft_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    checkHResult(hr, "Failed to notify start of stream");
}

void AudioEncoder::setOutputType() {
    Microsoft::WRL::ComPtr<IMFMediaType> outType;
    HRESULT hr = MFCreateMediaType(&outType);
    checkHResult(hr, "Failed to create output media type");

    outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    outType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
    outType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate_);
    outType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channels_);
    outType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, bitrate_ / 8);
    outType->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0); // 0 = RAW AAC (no ADTS header)
    outType->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0x29); // AAC LC

    hr = mft_->SetOutputType(0, outType.Get(), 0);
    checkHResult(hr, "Failed to set MFT output type");
}

void AudioEncoder::setInputType() {
    Microsoft::WRL::ComPtr<IMFMediaType> inType;
    HRESULT hr = MFCreateMediaType(&inType);
    checkHResult(hr, "Failed to create input media type");

    inType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    inType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    inType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate_);
    inType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channels_);
    inType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16); // MF AAC encoder prefers 16-bit PCM
    inType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, channels_ * 2);
    inType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, sampleRate_ * channels_ * 2);
    inType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

    hr = mft_->SetInputType(0, inType.Get(), 0);
    checkHResult(hr, "Failed to set MFT input type");
}

std::vector<uint8_t> AudioEncoder::encode(std::span<const float> pcmSamples) {
    if (pcmSamples.empty()) return {};

    pcmConversionBuffer_.resize(pcmSamples.size());
    for (size_t i = 0; i < pcmSamples.size(); ++i) {
        float f = pcmSamples[i];
        f = std::clamp(f, -1.0f, 1.0f);
        pcmConversionBuffer_[i] = static_cast<int16_t>(f * 32767.0f);
    }

    const DWORD cbData = static_cast<DWORD>(pcmConversionBuffer_.size() * sizeof(int16_t));

    Microsoft::WRL::ComPtr<IMFSample> sample;
    HRESULT hr = MFCreateSample(&sample);
    checkHResult(hr, "Failed to create input sample");

    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
    hr = MFCreateMemoryBuffer(cbData, &buffer);
    checkHResult(hr, "Failed to create memory buffer");

    BYTE* pData = nullptr;
    DWORD maxLength = 0;
    hr = buffer->Lock(&pData, &maxLength, nullptr);
    checkHResult(hr, "Failed to lock buffer");

    std::memcpy(pData, pcmConversionBuffer_.data(), cbData);
    buffer->Unlock();

    hr = buffer->SetCurrentLength(cbData);
    checkHResult(hr, "Failed to set buffer length");

    hr = sample->AddBuffer(buffer.Get());
    checkHResult(hr, "Failed to add buffer to sample");

    LONGLONG duration = (LONGLONG)pcmSamples.size() * 10000000LL / (sampleRate_ * channels_);
    sample->SetSampleDuration(duration);
    sample->SetSampleTime(inputSampleTime_);
    inputSampleTime_ += duration;

    hr = mft_->ProcessInput(inputStreamId_, sample.Get(), 0);
    if (hr == MF_E_NOTACCEPTING) {
        // MFT has enough input, process output first
        auto result = processOutput();
        hr = mft_->ProcessInput(inputStreamId_, sample.Get(), 0);
        checkHResult(hr, "Failed to process input after draining");
        auto moreResult = processOutput();
        result.insert(result.end(), moreResult.begin(), moreResult.end());
        return result;
    }
    checkHResult(hr, "Failed to process input");

    return processOutput();
}

std::vector<uint8_t> AudioEncoder::processOutput() {
    std::vector<uint8_t> outputData;
    MFT_OUTPUT_DATA_BUFFER outputBuffer = {};
    outputBuffer.dwStreamID = outputStreamId_;
    
    Microsoft::WRL::ComPtr<IMFSample> sample;
    HRESULT hr = MFCreateSample(&sample);
    if (FAILED(hr)) return outputData;

    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
    hr = MFCreateMemoryBuffer(65536, &buffer);
    if (FAILED(hr)) return outputData;

    sample->AddBuffer(buffer.Get());
    outputBuffer.pSample = sample.Get();

    DWORD status = 0;
    while (true) {
        hr = mft_->ProcessOutput(0, 1, &outputBuffer, &status);
        if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
            break;
        } else if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
            continue;
        }
        checkHResult(hr, "Failed to process output");

        Microsoft::WRL::ComPtr<IMFMediaBuffer> outBuffer;
        if (SUCCEEDED(outputBuffer.pSample->ConvertToContiguousBuffer(&outBuffer))) {
            BYTE* pData = nullptr;
            DWORD cbData = 0;
            if (SUCCEEDED(outBuffer->Lock(&pData, nullptr, &cbData))) {
                outputData.insert(outputData.end(), pData, pData + cbData);
                outBuffer->Unlock();
            }
        }

        if (outputBuffer.pEvents) {
            outputBuffer.pEvents->Release();
            outputBuffer.pEvents = nullptr;
        }
    }

    if (outputBuffer.pEvents) {
        outputBuffer.pEvents->Release();
    }

    return outputData;
}

std::vector<uint8_t> AudioEncoder::flush() {
    mft_->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
    return processOutput();
}

std::vector<uint8_t> AudioEncoder::getAudioSpecificConfig() const {
    uint8_t sampleRateIdx = 3; // 48000 Hz default
    switch (sampleRate_) {
        case 96000: sampleRateIdx = 0; break;
        case 88200: sampleRateIdx = 1; break;
        case 64000: sampleRateIdx = 2; break;
        case 48000: sampleRateIdx = 3; break;
        case 44100: sampleRateIdx = 4; break;
        case 32000: sampleRateIdx = 5; break;
        case 24000: sampleRateIdx = 6; break;
        case 22050: sampleRateIdx = 7; break;
        case 16000: sampleRateIdx = 8; break;
        case 12000: sampleRateIdx = 9; break;
        case 11025: sampleRateIdx = 10; break;
        case 8000:  sampleRateIdx = 11; break;
        case 7350:  sampleRateIdx = 12; break;
    }
    
    std::vector<uint8_t> asc(2);
    uint8_t objectType = 2; // AAC-LC
    asc[0] = (objectType << 3) | (sampleRateIdx >> 1);
    asc[1] = ((sampleRateIdx & 1) << 7) | ((channels_ & 0x0F) << 3);
    return asc;
}

} // namespace lightrec::audio
