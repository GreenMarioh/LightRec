#include "RingBuffer.h"
#include <cstring>
#include <algorithm>

namespace lightrec::clip {

RingBuffer::RingBuffer(std::chrono::seconds maxDuration, size_t bufferSize)
    : max_duration_(maxDuration)
    , buffer_size_(bufferSize)
    , buffer_(bufferSize)
    , write_index_(0)
    , read_index_(0)
{
}

void RingBuffer::push(const EncodedPacket& packet) {
    const uint32_t header_size = sizeof(PacketHeader);
    const uint32_t payload_size = static_cast<uint32_t>(packet.data.size() + packet.sps.size() + packet.pps.size());
    const uint32_t total_size = header_size + payload_size;

    // If packet size exceeds the buffer size, we cannot store it at all.
    if (total_size > buffer_size_) {
        return;
    }

    size_t w = write_index_.load(std::memory_order_relaxed);
    size_t r = read_index_.load(std::memory_order_relaxed);

    // 1. Size-based eviction
    // Keep evicting until we have enough space: buffer_size_ - (w - r) >= total_size
    while (w - r + total_size > buffer_size_) {
        if (r == w) {
            break;
        }
        PacketHeader oldest_header;
        copy_from_buffer(r, &oldest_header, sizeof(PacketHeader));
        
        // Advance read_index_ by the total size of the oldest packet
        r += oldest_header.total_size;
        read_index_.store(r, std::memory_order_release);
    }

    // 2. Time-based eviction
    // Evict packets older than max_duration_ relative to the new packet's PTS.
    // 1 second = 10,000,000 units of 100ns
    const int64_t max_duration_ns = max_duration_.count() * 10000000LL;
    while (r < w) {
        PacketHeader oldest_header;
        copy_from_buffer(r, &oldest_header, sizeof(PacketHeader));
        
        if (packet.pts - oldest_header.pts > max_duration_ns) {
            r += oldest_header.total_size;
            read_index_.store(r, std::memory_order_release);
        } else {
            break;
        }
    }

    // Ensure all read_index_ updates are visible before we write to the buffer
    std::atomic_thread_fence(std::memory_order_seq_cst);

    // 3. Write packet header & payloads
    PacketHeader header;
    header.total_size = total_size;
    header.pts = packet.pts;
    header.duration = packet.duration;
    header.is_idr = packet.isIDR ? 1 : 0;
    header.data_size = static_cast<uint32_t>(packet.data.size());
    header.sps_size = static_cast<uint32_t>(packet.sps.size());
    header.pps_size = static_cast<uint32_t>(packet.pps.size());

    size_t current_w = w;
    copy_to_buffer(current_w, &header, sizeof(PacketHeader));
    current_w += sizeof(PacketHeader);

    if (header.data_size > 0) {
        copy_to_buffer(current_w, packet.data.data(), header.data_size);
        current_w += header.data_size;
    }
    if (header.sps_size > 0) {
        copy_to_buffer(current_w, packet.sps.data(), header.sps_size);
        current_w += header.sps_size;
    }
    if (header.pps_size > 0) {
        copy_to_buffer(current_w, packet.pps.data(), header.pps_size);
        current_w += header.pps_size;
    }

    // Advance write_index_ with release semantics
    write_index_.store(w + total_size, std::memory_order_release);
}

std::vector<EncodedPacket> RingBuffer::snapshot() const {
    std::vector<EncodedPacket> result;
    const int max_retries = 10;

    for (int retry = 0; retry < max_retries; ++retry) {
        result.clear();

        size_t r = read_index_.load(std::memory_order_acquire);
        size_t w = write_index_.load(std::memory_order_acquire);

        if (r == w) {
            return result;
        }

        bool success = true;
        size_t current_r = r;
        while (current_r < w) {
            PacketHeader header;
            copy_from_buffer(current_r, &header, sizeof(PacketHeader));

            // Sanity checks on header in case we read partially overwritten/corrupt values
            if (header.total_size == 0 || header.total_size > buffer_size_) {
                success = false;
                break;
            }

            EncodedPacket pkt;
            pkt.pts = header.pts;
            pkt.duration = header.duration;
            pkt.isIDR = (header.is_idr != 0);

            size_t payload_offset = current_r + sizeof(PacketHeader);

            if (header.data_size > 0) {
                pkt.data.resize(header.data_size);
                copy_from_buffer(payload_offset, pkt.data.data(), header.data_size);
                payload_offset += header.data_size;
            }
            if (header.sps_size > 0) {
                pkt.sps.resize(header.sps_size);
                copy_from_buffer(payload_offset, pkt.sps.data(), header.sps_size);
                payload_offset += header.sps_size;
            }
            if (header.pps_size > 0) {
                pkt.pps.resize(header.pps_size);
                copy_from_buffer(payload_offset, pkt.pps.data(), header.pps_size);
                payload_offset += header.pps_size;
            }

            result.push_back(std::move(pkt));
            current_r += header.total_size;
        }

        // Ensure all buffer reads are completed before loading r_end
        std::atomic_thread_fence(std::memory_order_seq_cst);

        // Verify that the writer hasn't advanced the read pointer (evicted any packets)
        // and that no corruption was detected during copying.
        size_t r_end = read_index_.load(std::memory_order_acquire);
        if (success && (r_end == r)) {
            return result;
        }
    }

    return {};
}

void RingBuffer::clear() {
    write_index_.store(0, std::memory_order_release);
    read_index_.store(0, std::memory_order_release);
}

size_t RingBuffer::memoryUsage() const {
    return buffer_.size() + sizeof(RingBuffer);
}

void RingBuffer::copy_to_buffer(size_t idx, const void* src, size_t size) {
    if (size == 0) return;
    const uint8_t* src_bytes = static_cast<const uint8_t*>(src);
    size_t offset = idx % buffer_size_;
    if (offset + size <= buffer_size_) {
        std::memcpy(buffer_.data() + offset, src_bytes, size);
    } else {
        size_t part1 = buffer_size_ - offset;
        size_t part2 = size - part1;
        std::memcpy(buffer_.data() + offset, src_bytes, part1);
        std::memcpy(buffer_.data(), src_bytes + part1, part2);
    }
}

void RingBuffer::copy_from_buffer(size_t idx, void* dst, size_t size) const {
    if (size == 0) return;
    uint8_t* dst_bytes = static_cast<uint8_t*>(dst);
    size_t offset = idx % buffer_size_;
    if (offset + size <= buffer_size_) {
        std::memcpy(dst_bytes, buffer_.data() + offset, size);
    } else {
        size_t part1 = buffer_size_ - offset;
        size_t part2 = size - part1;
        std::memcpy(dst_bytes, buffer_.data() + offset, part1);
        std::memcpy(dst_bytes + part1, buffer_.data(), part2);
    }
}

} // namespace lightrec::clip
