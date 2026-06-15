#pragma once

#include "EncodedPacket.h"
#include <vector>
#include <atomic>
#include <chrono>

namespace lightrec::clip {

class RingBuffer {
public:
    // maxDuration: the time window of packets to retain (e.g. 15, 30, 60, 120 sec)
    // bufferSize: the pre-allocated size of the circular byte buffer in bytes
    RingBuffer(std::chrono::seconds maxDuration, size_t bufferSize);
    ~RingBuffer() = default;

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
    RingBuffer(RingBuffer&&) = delete;
    RingBuffer& operator=(RingBuffer&&) = delete;

    // Pushes an EncodedPacket into the buffer.
    // Lock-free and allocation-free. Evicts oldest packets when full or too old.
    void push(const EncodedPacket& packet);

    // Snapshots the buffer's contents. Safe to call concurrently with push.
    std::vector<EncodedPacket> snapshot() const;

    // Resets the read and write pointers.
    void clear();

    // Returns the total pre-allocated memory size.
    size_t memoryUsage() const;

private:
    struct PacketHeader {
        uint32_t total_size;
        int64_t pts;
        int64_t duration;
        uint32_t is_idr;
        uint32_t data_size;
        uint32_t sps_size;
        uint32_t pps_size;
    };

    void copy_to_buffer(size_t idx, const void* src, size_t size);
    void copy_from_buffer(size_t idx, void* dst, size_t size) const;

    const std::chrono::seconds max_duration_;
    const size_t buffer_size_;
    std::vector<uint8_t> buffer_;

    std::atomic<size_t> write_index_{0};
    std::atomic<size_t> read_index_{0};
};

} // namespace lightrec::clip
