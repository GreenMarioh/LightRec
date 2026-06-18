#pragma once
#include <atomic>
#include <cassert>
#include <cstddef>
#include <vector>
#include <new>

namespace LightRec::Util {

template <typename T>
class SPSCQueue {
public:
    explicit SPSCQueue(size_t capacity) : capacity_(capacity), head_(0), tail_(0) {
        assert(capacity > 0 && (capacity & (capacity - 1)) == 0 && "Capacity must be a power of two");
        buffer_.resize(capacity);
    }

    bool try_push(T value) {
        size_t current_tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = (current_tail + 1) & (capacity_ - 1);

        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }

        buffer_[current_tail] = std::move(value);
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    bool try_pop(T& value) {
        size_t current_head = head_.load(std::memory_order_relaxed);

        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false;
        }

        value = std::move(buffer_[current_head]);
        head_.store((current_head + 1) & (capacity_ - 1), std::memory_order_release);
        return true;
    }

private:
    size_t capacity_;
    std::vector<T> buffer_;
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
};

} // namespace LightRec::Util
