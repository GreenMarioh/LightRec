#pragma once

#include <functional>
#include <cstdint>
#include <cstddef>

namespace lightrec::audio {

enum class AudioDeviceType {
    System,
    Microphone
};

class IAudioSource {
public:
    virtual ~IAudioSource() = default;

    // Start/stop capture sessions
    virtual void start() = 0;
    virtual void stop() = 0;

    // Set callback to receive mixed, interleaved float samples (48kHz stereo)
    // callback arguments: (const float* data, size_t sample_count, uint64_t timestamp_qpc)
    // - sample_count: total number of float elements in data (i.e. frames * channels)
    // - timestamp_qpc: QueryPerformanceCounter value matching the first sample
    using AudioCallback = std::function<void(const float* data, size_t sample_count, uint64_t timestamp_qpc)>;
    virtual void setCallback(AudioCallback callback) = 0;

    // Set volume level (0.0f = silent, 1.0f = unit gain, values above 1.0f are allowed)
    virtual void setVolume(AudioDeviceType type, float volume) = 0;
    virtual float getVolume(AudioDeviceType type) const = 0;

    // Enable/disable microphone capture
    virtual void setMicrophoneEnabled(bool enabled) = 0;
    virtual bool isMicrophoneEnabled() const = 0;
};

} // namespace lightrec::audio
