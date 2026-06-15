#pragma once

#include <vector>
#include <atomic>
#include <cstddef>
#include <algorithm>

namespace lightrec::audio {

/**
 * A lock-free, zero-allocation-during-run Single-Producer Single-Consumer (SPSC)
 * ring buffer for float audio samples.
 */
class AudioRingBuffer {
public:
    explicit AudioRingBuffer(size_t capacity) {
        // Round capacity up to the next power of two for fast bitwise wrapping
        capacity_ = 1;
        while (capacity_ < capacity) {
            capacity_ <<= 1;
        }
        mask_ = capacity_ - 1;
        buffer_.resize(capacity_, 0.0f);
        write_index_.store(0, std::memory_order_relaxed);
        read_index_.store(0, std::memory_order_relaxed);
    }

    ~AudioRingBuffer() = default;

    // Disable copy/move
    AudioRingBuffer(const AudioRingBuffer&) = delete;
    AudioRingBuffer& operator=(const AudioRingBuffer&) = delete;
    AudioRingBuffer(AudioRingBuffer&&) = delete;
    AudioRingBuffer& operator=(AudioRingBuffer&&) = delete;

    /**
     * Write samples to the ring buffer. Called only by the producer thread.
     * Returns the number of samples successfully written.
     */
    size_t write(const float* data, size_t count) {
        if (count == 0) return 0;

        const size_t write_idx = write_index_.load(std::memory_order_relaxed);
        const size_t read_idx = read_index_.load(std::memory_order_acquire);

        const size_t occupied = write_idx - read_idx;
        if (occupied >= capacity_) return 0; // Buffer is completely full

        const size_t available = capacity_ - occupied;
        const size_t to_write = std::min(count, available);

        for (size_t i = 0; i < to_write; ++i) {
            buffer_[(write_idx + i) & mask_] = data[i];
        }

        write_index_.store(write_idx + to_write, std::memory_order_release);
        return to_write;
    }

    /**
     * Read samples from the ring buffer. Called only by the consumer thread.
     * Returns the number of samples successfully read.
     */
    size_t read(float* destination, size_t count) {
        if (count == 0) return 0;

        const size_t read_idx = read_index_.load(std::memory_order_relaxed);
        const size_t write_idx = write_index_.load(std::memory_order_acquire);

        if (read_idx == write_idx) return 0; // Buffer is empty

        const size_t available = write_idx - read_idx;
        const size_t to_read = std::min(count, available);

        for (size_t i = 0; i < to_read; ++i) {
            destination[i] = buffer_[(read_idx + i) & mask_];
        }

        read_index_.store(read_idx + to_read, std::memory_order_release);
        return to_read;
    }

    /**
     * Get the number of samples currently available for reading.
     */
    size_t size() const {
        const size_t write_idx = write_index_.load(std::memory_order_acquire);
        const size_t read_idx = read_index_.load(std::memory_order_acquire);
        return (write_idx >= read_idx) ? (write_idx - read_idx) : 0;
    }

    /**
     * Clear the buffer. Should be called when both producer and consumer are synchronized.
     */
    void clear() {
        write_index_.store(0, std::memory_order_relaxed);
        read_index_.store(0, std::memory_order_relaxed);
    }

private:
    std::vector<float> buffer_;
    size_t capacity_;
    size_t mask_;
    alignas(64) std::atomic<size_t> write_index_;
    alignas(64) std::atomic<size_t> read_index_;
};

} // namespace lightrec::audio
