#include "AudioClipBuffer.h"
#include <algorithm>
#include <cstring>

namespace lightrec::clip {

AudioClipBuffer::AudioClipBuffer(std::chrono::seconds maxDuration, int sampleRate, int channels)
    : max_duration_(maxDuration)
    , sample_rate_(sampleRate)
    , channels_(channels)
{
    capacity_ = static_cast<size_t>(maxDuration.count()) * sampleRate * channels;
    buffer_.resize(capacity_, 0.0f);
}

void AudioClipBuffer::push(const float* data, size_t count, int64_t end_pts) {
    if (count == 0 || capacity_ == 0) return;

    std::lock_guard<std::mutex> lock(mutex_);

    int64_t block_duration_pts = (static_cast<int64_t>(count) * 10'000'000) / (sample_rate_ * channels_);
    int64_t start_pts = end_pts - block_duration_pts;

    if (!full_ && head_ == 0) {
        base_pts_ = start_pts;
    }

    if (count >= capacity_) {
        const float* src = data + (count - capacity_);
        std::copy(src, src + capacity_, buffer_.begin());
        head_ = 0;
        full_ = true;
        
        int64_t skipped_duration = (static_cast<int64_t>(count - capacity_) * 10'000'000) / (sample_rate_ * channels_);
        base_pts_ = start_pts + skipped_duration;
        return;
    }

    size_t space_at_end = capacity_ - head_;
    if (count <= space_at_end) {
        std::copy(data, data + count, buffer_.begin() + head_);
        head_ += count;
        if (head_ == capacity_) {
            head_ = 0;
            full_ = true;
        }
    } else {
        std::copy(data, data + space_at_end, buffer_.begin() + head_);
        size_t remaining = count - space_at_end;
        std::copy(data + space_at_end, data + count, buffer_.begin());
        head_ = remaining;
        full_ = true;
    }

    if (full_) {
        int64_t capacity_duration_pts = (static_cast<int64_t>(capacity_) * 10'000'000) / (sample_rate_ * channels_);
        base_pts_ = end_pts - capacity_duration_pts;
    }
}

AudioClipBuffer::Snapshot AudioClipBuffer::snapshot(int64_t start_pts, int64_t end_pts) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Snapshot snap;
    if (head_ == 0 && !full_) {
        snap.start_pts = start_pts;
        return snap;
    }

    int64_t buffer_start_pts = base_pts_;
    size_t current_size = full_ ? capacity_ : head_;
    int64_t current_duration_pts = (static_cast<int64_t>(current_size) * 10'000'000) / (sample_rate_ * channels_);
    int64_t buffer_end_pts = buffer_start_pts + current_duration_pts;

    if (end_pts <= buffer_start_pts || start_pts >= buffer_end_pts) {
        snap.start_pts = start_pts;
        return snap;
    }

    int64_t actual_start_pts = std::max(start_pts, buffer_start_pts);
    int64_t actual_end_pts = std::min(end_pts, buffer_end_pts);

    snap.start_pts = actual_start_pts;
    
    int64_t start_offset_pts = actual_start_pts - buffer_start_pts;
    int64_t end_offset_pts = actual_end_pts - buffer_start_pts;
    
    size_t start_idx_logical = (start_offset_pts * sample_rate_ * channels_) / 10'000'000;
    size_t end_idx_logical = (end_offset_pts * sample_rate_ * channels_) / 10'000'000;
    
    start_idx_logical = (start_idx_logical / channels_) * channels_;
    end_idx_logical = (end_idx_logical / channels_) * channels_;
    
    if (end_idx_logical <= start_idx_logical) {
        return snap;
    }
    
    size_t samples_to_copy = end_idx_logical - start_idx_logical;
    snap.data.resize(samples_to_copy);
    
    size_t read_head = full_ ? head_ : 0;
    size_t physical_start = (read_head + start_idx_logical) % capacity_;
    
    size_t space_at_end = capacity_ - physical_start;
    if (samples_to_copy <= space_at_end) {
        std::copy(buffer_.begin() + physical_start, buffer_.begin() + physical_start + samples_to_copy, snap.data.begin());
    } else {
        std::copy(buffer_.begin() + physical_start, buffer_.end(), snap.data.begin());
        size_t remaining = samples_to_copy - space_at_end;
        std::copy(buffer_.begin(), buffer_.begin() + remaining, snap.data.begin() + space_at_end);
    }

    return snap;
}

size_t AudioClipBuffer::memoryUsage() const {
    return buffer_.capacity() * sizeof(float);
}

} // namespace lightrec::clip
