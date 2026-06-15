#include "WASAPICapture.h"
#include <initguid.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>

// Private copies of KSDATAFORMAT_SUBTYPE GUIDs to guarantee linking on MinGW/MSYS2
DEFINE_GUID(LIGHTREC_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(LIGHTREC_KSDATAFORMAT_SUBTYPE_PCM, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

namespace lightrec::audio {

// Helper to parse format details from WAVEFORMATEX
struct AudioFormat {
    uint32_t sampleRate{0};
    uint16_t channels{0};
    uint16_t bitsPerSample{0};
    bool isFloat{false};
};

static AudioFormat parseFormat(const WAVEFORMATEX* wfx) {
    AudioFormat fmt;
    fmt.sampleRate = wfx->nSamplesPerSec;
    fmt.channels = wfx->nChannels;
    fmt.bitsPerSample = wfx->wBitsPerSample;
    fmt.isFloat = false;

    if (wfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        fmt.isFloat = true;
    } else if (wfx->wFormatTag == WAVE_FORMAT_PCM) {
        fmt.isFloat = false;
    } else if (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const auto* wfe = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wfx);
        if (wfe->SubFormat == LIGHTREC_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
            fmt.isFloat = true;
        } else if (wfe->SubFormat == LIGHTREC_KSDATAFORMAT_SUBTYPE_PCM) {
            fmt.isFloat = false;
        }
    }
    return fmt;
}


// Convert arbitrary raw audio buffer to normalized float vector
static void convertToFloat(const uint8_t* rawData, size_t sampleCount, const AudioFormat& format, std::vector<float>& outFloats) {
    outFloats.resize(sampleCount);
    if (format.isFloat) {
        if (format.bitsPerSample == 32) {
            std::memcpy(outFloats.data(), rawData, sampleCount * sizeof(float));
        } else if (format.bitsPerSample == 64) {
            const double* src = reinterpret_cast<const double*>(rawData);
            for (size_t i = 0; i < sampleCount; ++i) {
                outFloats[i] = static_cast<float>(src[i]);
            }
        }
    } else {
        if (format.bitsPerSample == 16) {
            const int16_t* src = reinterpret_cast<const int16_t*>(rawData);
            for (size_t i = 0; i < sampleCount; ++i) {
                outFloats[i] = static_cast<float>(src[i]) / 32768.0f;
            }
        } else if (format.bitsPerSample == 32) {
            const int32_t* src = reinterpret_cast<const int32_t*>(rawData);
            for (size_t i = 0; i < sampleCount; ++i) {
                outFloats[i] = static_cast<float>(src[i]) / 2147483648.0f;
            }
        } else if (format.bitsPerSample == 24) {
            for (size_t i = 0; i < sampleCount; ++i) {
                // Read 3 bytes (little endian sign extended)
                int32_t val = 0;
                val |= rawData[i * 3 + 0] << 8;
                val |= rawData[i * 3 + 1] << 16;
                val |= rawData[i * 3 + 2] << 24;
                outFloats[i] = static_cast<float>(val) / 2147483648.0f;
            }
        } else if (format.bitsPerSample == 8) {
            for (size_t i = 0; i < sampleCount; ++i) {
                outFloats[i] = (static_cast<float>(rawData[i]) - 128.0f) / 128.0f;
            }
        }
    }
}

// Resample format and upmix/downmix channels to stereo 48kHz float
static void resampleAndMixChannels(
    const std::vector<float>& srcFloats,
    const AudioFormat& srcFormat,
    std::vector<float>& destFloats,
    double& phase
) {
    constexpr uint32_t destRate = 48000;
    constexpr uint32_t destChannels = 2;

    if (srcFloats.empty() || srcFormat.channels == 0) {
        destFloats.clear();
        return;
    }

    size_t srcFrames = srcFloats.size() / srcFormat.channels;
    if (srcFrames == 0) {
        destFloats.clear();
        return;
    }

    double rateRatio = static_cast<double>(srcFormat.sampleRate) / destRate;

    // Estimate output frame count
    size_t destFrames = 0;
    double tempPhase = phase;
    while (tempPhase < srcFrames) {
        destFrames++;
        tempPhase += rateRatio;
    }

    destFloats.resize(destFrames * destChannels);

    for (size_t f = 0; f < destFrames; ++f) {
        size_t idx1 = static_cast<size_t>(phase);
        size_t idx2 = idx1 + 1;
        if (idx2 >= srcFrames) {
            idx2 = srcFrames - 1;
        }
        double frac = phase - idx1;

        float left1 = 0.0f;
        float right1 = 0.0f;
        float left2 = 0.0f;
        float right2 = 0.0f;

        // Map channels for frame 1
        if (srcFormat.channels == 1) {
            left1 = srcFloats[idx1];
            right1 = srcFloats[idx1];
        } else {
            left1 = srcFloats[idx1 * srcFormat.channels + 0];
            right1 = srcFloats[idx1 * srcFormat.channels + 1];
        }

        // Map channels for frame 2
        if (srcFormat.channels == 1) {
            left2 = srcFloats[idx2];
            right2 = srcFloats[idx2];
        } else {
            left2 = srcFloats[idx2 * srcFormat.channels + 0];
            right2 = srcFloats[idx2 * srcFormat.channels + 1];
        }

        // Linear interpolation
        destFloats[f * destChannels + 0] = static_cast<float>((1.0 - frac) * left1 + frac * left2);
        destFloats[f * destChannels + 1] = static_cast<float>((1.0 - frac) * right1 + frac * right2);

        phase += rateRatio;
    }

    phase -= srcFrames;
    if (phase < 0.0) {
        phase = 0.0;
    }
}

WASAPICapture::WASAPICapture()
    : micQueue_(262144) // ~2.7 seconds of 48kHz stereo float buffer
{
    stopEvent_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
}

WASAPICapture::~WASAPICapture() {
    stop();
    if (stopEvent_) {
        CloseHandle(stopEvent_);
    }
    cleanupSystemAudio();
    cleanupMicrophone();
}

void WASAPICapture::start() {
    if (running_.load()) return;
    running_.store(true);

    ResetEvent(stopEvent_);

    // Initialize System Audio Loopback
    if (initSystemAudio()) {
        systemThread_ = std::jthread(&WASAPICapture::runSystemCapture, this);
    }

    // Initialize Microphone if enabled
    if (micEnabled_.load()) {
        if (initMicrophone()) {
            micThread_ = std::jthread(&WASAPICapture::runMicCapture, this);
        }
    }
}

void WASAPICapture::stop() {
    if (!running_.load()) return;
    running_.store(false);

    if (stopEvent_) {
        SetEvent(stopEvent_);
    }

    if (systemThread_.joinable()) {
        systemThread_.join();
    }
    if (micThread_.joinable()) {
        micThread_.join();
    }

    if (systemClient_) {
        systemClient_->Stop();
    }
    if (micClient_) {
        micClient_->Stop();
    }

    cleanupSystemAudio();
    cleanupMicrophone();
    micQueue_.clear();
}

void WASAPICapture::setCallback(AudioCallback callback) {
    callback_ = std::move(callback);
}

void WASAPICapture::setVolume(AudioDeviceType type, float volume) {
    if (type == AudioDeviceType::System) {
        systemVolume_.store(volume);
    } else {
        micVolume_.store(volume);
    }
}

float WASAPICapture::getVolume(AudioDeviceType type) const {
    if (type == AudioDeviceType::System) {
        return systemVolume_.load();
    } else {
        return micVolume_.load();
    }
}

void WASAPICapture::setMicrophoneEnabled(bool enabled) {
    bool wasEnabled = micEnabled_.exchange(enabled);
    if (running_.load()) {
        if (enabled && !wasEnabled) {
            // Dynamically start mic capture
            if (initMicrophone()) {
                micThread_ = std::jthread(&WASAPICapture::runMicCapture, this);
            }
        } else if (!enabled && wasEnabled) {
            // Dynamically stop mic capture
            if (micClient_) {
                micClient_->Stop();
            }
            if (micThread_.joinable()) {
                micThread_.join();
            }
            cleanupMicrophone();
        }
    }
}

bool WASAPICapture::isMicrophoneEnabled() const {
    return micEnabled_.load();
}

bool WASAPICapture::initSystemAudio() {
    cleanupSystemAudio();

    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    if (FAILED(hr)) return false;

    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &systemDevice_);
    if (FAILED(hr)) return false;

    hr = systemDevice_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&systemClient_);
    if (FAILED(hr)) return false;

    hr = systemClient_->GetMixFormat(&systemFormat_);
    if (FAILED(hr)) return false;

    // Loopback initialization in shared mode with event callback
    hr = systemClient_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        0, 0, systemFormat_, nullptr
    );
    if (FAILED(hr)) return false;

    systemEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!systemEvent_) return false;

    hr = systemClient_->SetEventHandle(systemEvent_);
    if (FAILED(hr)) return false;

    hr = systemClient_->GetService(__uuidof(IAudioCaptureClient), (void**)&systemCaptureClient_);
    if (FAILED(hr)) return false;

    return true;
}

bool WASAPICapture::initMicrophone() {
    cleanupMicrophone();

    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    if (FAILED(hr)) return false;

    hr = enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &micDevice_);
    if (FAILED(hr)) return false;

    hr = micDevice_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&micClient_);
    if (FAILED(hr)) return false;

    hr = micClient_->GetMixFormat(&micFormat_);
    if (FAILED(hr)) return false;

    // Shared capture mode with event callback
    hr = micClient_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        0, 0, micFormat_, nullptr
    );
    if (FAILED(hr)) return false;

    micEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!micEvent_) return false;

    hr = micClient_->SetEventHandle(micEvent_);
    if (FAILED(hr)) return false;

    hr = micClient_->GetService(__uuidof(IAudioCaptureClient), (void**)&micCaptureClient_);
    if (FAILED(hr)) return false;

    return true;
}

void WASAPICapture::cleanupSystemAudio() {
    systemCaptureClient_.Reset();
    systemClient_.Reset();
    systemDevice_.Reset();
    if (systemEvent_) {
        CloseHandle(systemEvent_);
        systemEvent_ = nullptr;
    }
    if (systemFormat_) {
        CoTaskMemFree(systemFormat_);
        systemFormat_ = nullptr;
    }
}

void WASAPICapture::cleanupMicrophone() {
    micCaptureClient_.Reset();
    micClient_.Reset();
    micDevice_.Reset();
    if (micEvent_) {
        CloseHandle(micEvent_);
        micEvent_ = nullptr;
    }
    if (micFormat_) {
        CoTaskMemFree(micFormat_);
        micFormat_ = nullptr;
    }
}

void WASAPICapture::runMicCapture() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    struct ComGuard {
        HRESULT initHr;
        ~ComGuard() { if (SUCCEEDED(initHr)) CoUninitialize(); }
    } comGuard{hr};

    if (FAILED(micClient_->Start())) return;

    HANDLE waitEvents[] = { stopEvent_, micEvent_ };

    // Pre-allocated buffers to prevent dynamic allocation on hot path
    std::vector<float> rawBuffer;
    std::vector<float> resampledBuffer;
    AudioFormat format = parseFormat(micFormat_);

    while (running_.load()) {
        DWORD waitResult = WaitForMultipleObjects(2, waitEvents, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0) {
            break;
        }

        UINT32 packetLength = 0;
        hr = micCaptureClient_->GetNextPacketSize(&packetLength);
        while (SUCCEEDED(hr) && packetLength > 0) {
            BYTE* pData = nullptr;
            UINT32 numFramesRead = 0;
            DWORD flags = 0;

            hr = micCaptureClient_->GetBuffer(&pData, &numFramesRead, &flags, nullptr, nullptr);
            if (SUCCEEDED(hr)) {
                size_t sampleCount = numFramesRead * format.channels;
                rawBuffer.resize(sampleCount);

                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    std::fill(rawBuffer.begin(), rawBuffer.end(), 0.0f);
                } else {
                    convertToFloat(pData, sampleCount, format, rawBuffer);
                }

                // Resample and mix channels into 48kHz Stereo float
                resampleAndMixChannels(rawBuffer, format, resampledBuffer, micResampler_.phase);

                // Write resampled stereo samples to SPSC queue
                micQueue_.write(resampledBuffer.data(), resampledBuffer.size());

                micCaptureClient_->ReleaseBuffer(numFramesRead);
            }
            hr = micCaptureClient_->GetNextPacketSize(&packetLength);
        }
    }
}

void WASAPICapture::runSystemCapture() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    struct ComGuard {
        HRESULT initHr;
        ~ComGuard() { if (SUCCEEDED(initHr)) CoUninitialize(); }
    } comGuard{hr};

    if (FAILED(systemClient_->Start())) return;

    HANDLE waitEvents[] = { stopEvent_, systemEvent_ };

    LARGE_INTEGER qpcFreq;
    QueryPerformanceFrequency(&qpcFreq);
    double qpcFreqD = static_cast<double>(qpcFreq.QuadPart);

    uint64_t lastQpc = 0;
    AudioFormat format = parseFormat(systemFormat_);

    // Pre-allocate buffers for performance
    std::vector<float> rawBuffer;
    std::vector<float> systemResampled;
    std::vector<float> micBuffer;
    std::vector<float> mixedBuffer;

    while (running_.load()) {
        // Wait up to 10ms for system audio. If silent, we time out and generate silence.
        DWORD waitResult = WaitForMultipleObjects(2, waitEvents, FALSE, 10);
        if (waitResult == WAIT_OBJECT_0) {
            break;
        }

        uint64_t currentQpc = 0;
        {
            LARGE_INTEGER qpc;
            QueryPerformanceCounter(&qpc);
            currentQpc = qpc.QuadPart;
        }

        if (lastQpc == 0) {
            lastQpc = currentQpc;
        }

        bool hasData = false;
        UINT32 packetLength = 0;
        hr = systemCaptureClient_->GetNextPacketSize(&packetLength);

        // Keep track of the actual processed loop
        while (SUCCEEDED(hr) && packetLength > 0) {
            BYTE* pData = nullptr;
            UINT32 numFramesRead = 0;
            DWORD flags = 0;
            UINT64 qpcPosition = 0;

            hr = systemCaptureClient_->GetNextPacketSize(&packetLength);
            if (FAILED(hr) || packetLength == 0) break;

            hr = systemCaptureClient_->GetBuffer(&pData, &numFramesRead, &flags, nullptr, &qpcPosition);
            if (SUCCEEDED(hr)) {
                hasData = true;
                if (qpcPosition == 0) {
                    qpcPosition = currentQpc;
                }

                // 1. Fill any timeline gap prior to this packet with silence
                if (qpcPosition > lastQpc) {
                    uint64_t gapQpc = qpcPosition - lastQpc;
                    double gapSec = static_cast<double>(gapQpc) / qpcFreqD;
                    size_t gapFrames = static_cast<size_t>(gapSec * 48000.0);
                    if (gapFrames > 5 && gapFrames < 240000) { // Limit to max 5s of gap to avoid runaway allocation
                        size_t sampleCount = gapFrames * 2;
                        mixedBuffer.resize(sampleCount);
                        std::fill(mixedBuffer.begin(), mixedBuffer.end(), 0.0f);

                        // Mix mic if active
                        if (micEnabled_.load()) {
                            micBuffer.resize(sampleCount);
                            size_t readCount = micQueue_.read(micBuffer.data(), sampleCount);
                            if (readCount < sampleCount) {
                                std::fill(micBuffer.begin() + readCount, micBuffer.end(), 0.0f);
                            }
                            float mV = micVolume_.load();
                            for (size_t i = 0; i < sampleCount; ++i) {
                                mixedBuffer[i] = std::clamp(micBuffer[i] * mV, -1.0f, 1.0f);
                            }
                        }

                        if (callback_) {
                            callback_(mixedBuffer.data(), sampleCount, lastQpc);
                        }
                    }
                    lastQpc = qpcPosition;
                }

                // 2. Process current loopback packet
                size_t sampleCount = numFramesRead * format.channels;
                rawBuffer.resize(sampleCount);

                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    std::fill(rawBuffer.begin(), rawBuffer.end(), 0.0f);
                } else {
                    convertToFloat(pData, sampleCount, format, rawBuffer);
                }

                resampleAndMixChannels(rawBuffer, format, systemResampled, systemResampler_.phase);

                size_t outSamples = systemResampled.size();
                mixedBuffer.resize(outSamples);

                // Read matching amount of mic samples
                if (micEnabled_.load()) {
                    micBuffer.resize(outSamples);
                    size_t readCount = micQueue_.read(micBuffer.data(), outSamples);
                    if (readCount < outSamples) {
                        std::fill(micBuffer.begin() + readCount, micBuffer.end(), 0.0f);
                    }
                } else {
                    micBuffer.resize(outSamples);
                    std::fill(micBuffer.begin(), micBuffer.end(), 0.0f);
                }

                // Mix and apply volume
                float sV = systemVolume_.load();
                float mV = micVolume_.load();
                for (size_t i = 0; i < outSamples; ++i) {
                    float val = systemResampled[i] * sV + micBuffer[i] * mV;
                    mixedBuffer[i] = std::clamp(val, -1.0f, 1.0f);
                }

                if (callback_ && outSamples > 0) {
                    callback_(mixedBuffer.data(), outSamples, lastQpc);
                }

                // Advance clock
                size_t outFrames = outSamples / 2;
                lastQpc += static_cast<uint64_t>((static_cast<double>(outFrames) / 48000.0) * qpcFreqD);

                systemCaptureClient_->ReleaseBuffer(numFramesRead);
            }
            hr = systemCaptureClient_->GetNextPacketSize(&packetLength);
        }

        // 3. Handle timeout / silence generation if no data arrived and 10ms elapsed
        if (!hasData) {
            uint64_t elapsedQpc = currentQpc - lastQpc;
            double elapsedSec = static_cast<double>(elapsedQpc) / qpcFreqD;
            if (elapsedSec >= 0.01) { // 10ms
                size_t silenceFrames = static_cast<size_t>(elapsedSec * 48000.0);
                if (silenceFrames > 0 && silenceFrames < 4800) { // Limit sanity check (max 100ms generated in a single timeout check)
                    size_t sampleCount = silenceFrames * 2;
                    mixedBuffer.resize(sampleCount);
                    std::fill(mixedBuffer.begin(), mixedBuffer.end(), 0.0f);

                    if (micEnabled_.load()) {
                        micBuffer.resize(sampleCount);
                        size_t readCount = micQueue_.read(micBuffer.data(), sampleCount);
                        if (readCount < sampleCount) {
                            std::fill(micBuffer.begin() + readCount, micBuffer.end(), 0.0f);
                        }
                        float mV = micVolume_.load();
                        for (size_t i = 0; i < sampleCount; ++i) {
                            mixedBuffer[i] = std::clamp(micBuffer[i] * mV, -1.0f, 1.0f);
                        }
                    }

                    if (callback_) {
                        callback_(mixedBuffer.data(), sampleCount, lastQpc);
                    }

                    lastQpc += static_cast<uint64_t>((static_cast<double>(silenceFrames) / 48000.0) * qpcFreqD);
                } else {
                    // Reset if elapsed time is invalid or too large
                    lastQpc = currentQpc;
                }
            }
        }
    }
}

} // namespace lightrec::audio
