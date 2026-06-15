#pragma once

#include <vector>
#include <chrono>
#include <mutex>
#include <cstdint>
#include <cstddef>

namespace lightrec::clip {

class AudioClipBuffer {
public:
    struct Snapshot {
        std::vector<float> data;
        int64_t start_pts = 0; // 100ns units
    };

    // maxDuration: the maximum length of time to keep in the buffer
    // sampleRate: typically 48000
    // channels: typically 2
    AudioClipBuffer(std::chrono::seconds maxDuration, int sampleRate, int channels);

    ~AudioClipBuffer() = default;
    
    // Non-copyable
    AudioClipBuffer(const AudioClipBuffer&) = delete;
    AudioClipBuffer& operator=(const AudioClipBuffer&) = delete;

    // Push audio samples into the circular buffer
    // end_pts is the PTS (in 100ns units) corresponding to the end of this sample block
    void push(const float* data, size_t count, int64_t end_pts);

    // Snapshot a time window from the buffer
    // start_pts: the requested start time (100ns units)
    // end_pts: the requested end time (100ns units)
    Snapshot snapshot(int64_t start_pts, int64_t end_pts) const;

    // Get the total pre-allocated memory size in bytes
    size_t memoryUsage() const;

private:
    std::chrono::seconds max_duration_;
    int sample_rate_;
    int channels_;
    
    size_t capacity_;
    std::vector<float> buffer_;
    
    mutable std::mutex mutex_;
    size_t head_ = 0;
    bool full_ = false;
    
    // The PTS of the sample at buffer_[head_]
    // If full_ is true, this is the oldest sample's PTS.
    // If full_ is false, it's the PTS of the first sample ever pushed (buffer_[0]).
    int64_t base_pts_ = 0;
};

} // namespace lightrec::clip
