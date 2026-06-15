#include "../../src/clip/RingBuffer.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <new>
#include <cstdlib>

using namespace lightrec::clip;

// Global allocation tracker to verify no dynamic allocations occur during gameplay (push)
static thread_local bool g_prevent_allocations = false;
static std::atomic<size_t> g_allocation_count{0};

void* operator new(size_t size) {
    if (g_prevent_allocations) {
        g_allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    return std::malloc(size);
}

void operator delete(void* ptr) noexcept {
    std::free(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    std::free(ptr);
}

void* operator new[](size_t size) {
    if (g_prevent_allocations) {
        g_allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    return std::malloc(size);
}

void operator delete[](void* ptr) noexcept {
    std::free(ptr);
}

void operator delete[](void* ptr, size_t) noexcept {
    std::free(ptr);
}

void test_basic_push_pop() {
    std::cout << "Running test_basic_push_pop..." << std::endl;
    RingBuffer rb(std::chrono::seconds(15), 1024);

    EncodedPacket pkt;
    pkt.data = {1, 2, 3, 4};
    pkt.pts = 1000;
    pkt.duration = 33;
    pkt.isIDR = true;
    pkt.sps = {5, 6};
    pkt.pps = {7};

    rb.push(pkt);

    auto snap = rb.snapshot();
    assert(snap.size() == 1);
    assert(snap[0].pts == 1000);
    assert(snap[0].duration == 33);
    assert(snap[0].isIDR == true);
    assert(snap[0].data == pkt.data);
    assert(snap[0].sps == pkt.sps);
    assert(snap[0].pps == pkt.pps);
    std::cout << "test_basic_push_pop passed." << std::endl;
}

void test_size_based_eviction() {
    std::cout << "Running test_size_based_eviction..." << std::endl;
    
    // Header size + 10 bytes = ~50 bytes per packet.
    // Set buffer size to 110 bytes (holds 2 packets).
    RingBuffer rb(std::chrono::seconds(15), 110);

    EncodedPacket pkt1;
    pkt1.data = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    pkt1.pts = 100;

    EncodedPacket pkt2;
    pkt2.data = pkt1.data;
    pkt2.pts = 200;

    EncodedPacket pkt3;
    pkt3.data = pkt1.data;
    pkt3.pts = 300;

    rb.push(pkt1);
    rb.push(pkt2);

    auto snap1 = rb.snapshot();
    assert(snap1.size() == 2);
    assert(snap1[0].pts == 100);
    assert(snap1[1].pts == 200);

    // This push should trigger size-based eviction of pkt1
    rb.push(pkt3);

    auto snap2 = rb.snapshot();
    assert(snap2.size() == 2);
    assert(snap2[0].pts == 200);
    assert(snap2[1].pts == 300);

    std::cout << "test_size_based_eviction passed." << std::endl;
}

void test_time_based_eviction() {
    std::cout << "Running test_time_based_eviction..." << std::endl;
    // Window is 5 seconds
    RingBuffer rb(std::chrono::seconds(5), 4096);

    EncodedPacket pkt1;
    pkt1.pts = 0; // 0 seconds

    EncodedPacket pkt2;
    pkt2.pts = 3 * 10000000LL; // 3 seconds later

    EncodedPacket pkt3;
    pkt3.pts = 6 * 10000000LL; // 6 seconds later

    rb.push(pkt1);
    rb.push(pkt2);

    auto snap1 = rb.snapshot();
    assert(snap1.size() == 2);

    // Pushing pkt3 (at 6s) should evict pkt1 (at 0s) because 6s - 0s = 6s > 5s
    rb.push(pkt3);

    auto snap2 = rb.snapshot();
    assert(snap2.size() == 2);
    assert(snap2[0].pts == 30000000LL);
    assert(snap2[1].pts == 60000000LL);

    std::cout << "test_time_based_eviction passed." << std::endl;
}

void test_no_allocations() {
    std::cout << "Running test_no_allocations..." << std::endl;
    RingBuffer rb(std::chrono::seconds(15), 1024);

    EncodedPacket pkt;
    pkt.data = {1, 2, 3, 4};
    pkt.pts = 1000;

    // Enable allocation tracker on the current thread
    g_prevent_allocations = true;
    g_allocation_count = 0;

    rb.push(pkt);

    g_prevent_allocations = false;

    assert(g_allocation_count.load() == 0);
    std::cout << "test_no_allocations passed (dynamic allocations = 0)." << std::endl;
}

void test_concurrent_push_snapshot() {
    std::cout << "Running test_concurrent_push_snapshot..." << std::endl;
    // 10 MB buffer
    RingBuffer rb(std::chrono::seconds(5), 10 * 1024 * 1024);

    std::atomic<bool> running{true};

    // Producer thread pushing packets at high speed
    std::thread producer([&]() {
        int64_t pts = 0;
        std::vector<uint8_t> data(1000, 0xA); // 1KB packets
        while (running) {
            EncodedPacket pkt;
            pkt.data = data;
            pkt.pts = pts++;
            pkt.duration = 166666; // 60 FPS
            pkt.isIDR = (pts % 30 == 0); // IDR keyframe every 30 frames
            rb.push(pkt);
            std::this_thread::yield();
        }
    });

    // Consumer thread taking snapshots and validating integrity
    std::thread consumer([&]() {
        while (running) {
            auto snap = rb.snapshot();
            if (!snap.empty()) {
                // Verify PTS monotonicity and non-corrupt packet contents
                int64_t last_pts = -1;
                for (const auto& pkt : snap) {
                    if (pkt.pts <= last_pts) {
                        std::fprintf(stderr, "Error: PTS monotonicity failed. Current PTS = %lld, Last PTS = %lld\n",
                                    (long long)pkt.pts, (long long)last_pts);
                        std::fflush(stderr);
                        assert(false);
                    }
                    last_pts = pkt.pts;
                    assert(pkt.data.size() == 1000);
                    for (size_t i = 0; i < pkt.data.size(); ++i) {
                        if (pkt.data[i] != 0xA) {
                            std::fprintf(stderr, "Error: pkt.data[%zu] = 0x%02X (expected 0x0A), PTS = %lld, duration = %lld, isIDR = %d, data_size = %zu, sps_size = %zu, pps_size = %zu\n",
                                        i, pkt.data[i], (long long)pkt.pts, (long long)pkt.duration, pkt.isIDR, pkt.data.size(), pkt.sps.size(), pkt.pps.size());
                            std::fprintf(stderr, "Surrounding bytes: ");
                            for (size_t j = (i >= 10 ? i - 10 : 0); j < std::min(pkt.data.size(), i + 10); ++j) {
                                std::fprintf(stderr, "%zu:0x%02X ", j, pkt.data[j]);
                            }
                            std::fprintf(stderr, "\n");
                            std::fflush(stderr);
                            assert(false);
                        }
                    }
                }
            }
            std::this_thread::yield();
        }
    });

    // Let them run for 500ms
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    running = false;

    producer.join();
    consumer.join();

    std::cout << "test_concurrent_push_snapshot passed." << std::endl;
}

int main() {
    test_basic_push_pop();
    test_size_based_eviction();
    test_time_based_eviction();
    test_no_allocations();
    test_concurrent_push_snapshot();
    std::cout << "All RingBuffer tests passed successfully!" << std::endl;
    return 0;
}
