#pragma once

#include "IAudioSource.h"
#include "AudioRingBuffer.h"

#include <windows.h>
#include <wrl/client.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

namespace lightrec::audio {

class WASAPICapture : public IAudioSource {
public:
    WASAPICapture();
    ~WASAPICapture() override;

    // IAudioSource implementation
    void start() override;
    void stop() override;
    void setCallback(AudioCallback callback) override;
    void setVolume(AudioDeviceType type, float volume) override;
    float getVolume(AudioDeviceType type) const override;
    void setMicrophoneEnabled(bool enabled) override;
    bool isMicrophoneEnabled() const override;

private:
    // Thread worker functions
    void runSystemCapture();
    void runMicCapture();

    // Helper functions for device initialization
    bool initSystemAudio();
    bool initMicrophone();
    void cleanupSystemAudio();
    void cleanupMicrophone();

    // Callback and state
    AudioCallback callback_;
    std::atomic<bool> running_{false};
    std::atomic<bool> micEnabled_{false};
    std::atomic<float> systemVolume_{1.0f};
    std::atomic<float> micVolume_{1.0f};

    // SPSC Queue for passing resampled mic samples to the system audio/mixer thread
    AudioRingBuffer micQueue_;

    // COM pointers for System Audio Loopback
    Microsoft::WRL::ComPtr<IMMDevice> systemDevice_;
    Microsoft::WRL::ComPtr<IAudioClient> systemClient_;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> systemCaptureClient_;
    HANDLE systemEvent_{nullptr};
    WAVEFORMATEX* systemFormat_{nullptr};

    // COM pointers for Microphone Capture
    Microsoft::WRL::ComPtr<IMMDevice> micDevice_;
    Microsoft::WRL::ComPtr<IAudioClient> micClient_;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> micCaptureClient_;
    HANDLE micEvent_{nullptr};
    WAVEFORMATEX* micFormat_{nullptr};

    // Control event to stop threads
    HANDLE stopEvent_{nullptr};

    // Worker threads
    std::jthread systemThread_;
    std::jthread micThread_;

    // Resampler/converter states
    struct ResamplerState {
        double phase{0.0};
        std::vector<float> channelBuffer; // Temporary storage for format conversion
    };
    ResamplerState micResampler_;
    ResamplerState systemResampler_;
};

} // namespace lightrec::audio
