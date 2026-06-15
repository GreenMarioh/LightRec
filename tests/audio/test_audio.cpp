#include "IAudioSource.h"
#include "AudioRingBuffer.h"
#include "WASAPICapture.h"

#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <atomic>
#include <windows.h>
#include <cmath>


using namespace lightrec::audio;

void test_ring_buffer() {
    std::cout << "Running test_ring_buffer..." << std::endl;

    // Buffer capacity will be rounded to 8 (next power of 2)
    AudioRingBuffer rb(6);

    // Test initially empty
    assert(rb.size() == 0);
    float temp[4] = {0};
    assert(rb.read(temp, 4) == 0);

    // Test successful write and read
    float input[] = { 0.1f, 0.2f, 0.3f, 0.4f };
    size_t written = rb.write(input, 4);
    assert(written == 4);
    assert(rb.size() == 4);

    float output[4] = {0};
    size_t readCount = rb.read(output, 4);
    assert(readCount == 4);
    assert(rb.size() == 0);
    assert(output[0] == 0.1f);
    assert(output[1] == 0.2f);
    assert(output[2] == 0.3f);
    assert(output[3] == 0.4f);

    // Test wrapping and capacity boundaries
    // Write 6 elements
    float input2[] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };
    assert(rb.write(input2, 6) == 6);
    assert(rb.size() == 6);

    // Write 4 more. Since capacity is 8, only 2 should fit
    float input3[] = { 7.0f, 8.0f, 9.0f, 10.0f };
    assert(rb.write(input3, 4) == 2); // 7.0 and 8.0 should be written
    assert(rb.size() == 8);

    // Read all 8 elements
    float output2[8] = {0};
    assert(rb.read(output2, 8) == 8);
    assert(rb.size() == 0);
    assert(output2[0] == 1.0f);
    assert(output2[5] == 6.0f);
    assert(output2[6] == 7.0f);
    assert(output2[7] == 8.0f);

    // Test clear
    assert(rb.write(input, 4) == 4);
    assert(rb.size() == 4);
    rb.clear();
    assert(rb.size() == 0);

    std::cout << "test_ring_buffer PASSED" << std::endl;
}

void test_wasapi_capture_properties() {
    std::cout << "Running test_wasapi_capture_properties..." << std::endl;

    WASAPICapture capture;

    // Test volume controls
    capture.setVolume(AudioDeviceType::System, 0.5f);
    assert(std::abs(capture.getVolume(AudioDeviceType::System) - 0.5f) < 1e-5f);

    capture.setVolume(AudioDeviceType::Microphone, 0.8f);
    assert(std::abs(capture.getVolume(AudioDeviceType::Microphone) - 0.8f) < 1e-5f);

    // Test microphone toggle state
    capture.setMicrophoneEnabled(true);
    assert(capture.isMicrophoneEnabled() == true);

    capture.setMicrophoneEnabled(false);
    assert(capture.isMicrophoneEnabled() == false);

    std::cout << "test_wasapi_capture_properties PASSED" << std::endl;
}

void test_wasapi_capture_integration() {
    std::cout << "Running test_wasapi_capture_integration..." << std::endl;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    struct ComGuard {
        HRESULT initHr;
        ~ComGuard() { if (SUCCEEDED(initHr)) CoUninitialize(); }
    } comGuard{hr};

    WASAPICapture capture;
    std::atomic<size_t> totalSamplesReceived{0};
    std::atomic<bool> formatValid{true};
    std::atomic<bool> timestampValid{true};
    uint64_t lastTimestamp = 0;

    capture.setCallback([&](const float* data, size_t sampleCount, uint64_t timestampQpc) {
        if (sampleCount == 0 || data == nullptr) {
            return;
        }

        // Output must be stereo (even sample count)
        if (sampleCount % 2 != 0) {
            formatValid.store(false);
        }

        // Verify that values are within standard float ranges or silent
        for (size_t i = 0; i < sampleCount; ++i) {
            if (std::isnan(data[i]) || std::isinf(data[i]) || data[i] < -1.1f || data[i] > 1.1f) {
                formatValid.store(false);
            }
        }

        totalSamplesReceived.fetch_add(sampleCount);
    });

    // We enable microphone capture as well to test both threads
    capture.setMicrophoneEnabled(true);

    std::cout << "Starting WASAPI capture (2 seconds)..." << std::endl;
    capture.start();

    // Sleep for 2 seconds to gather samples
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "Stopping WASAPI capture..." << std::endl;
    capture.stop();

    size_t samples = totalSamplesReceived.load();
    std::cout << "Received " << samples << " samples during 2 seconds." << std::endl;

    // Verify format and that some samples were received
    assert(formatValid.load() == true);
    // Since we generate silence on timeout, even if no sound is playing, we MUST receive samples.
    // 48000 samples/sec * 2 channels * 2 sec = 192000 samples.
    // Let's verify we received at least some samples (e.g. > 10000) to confirm the thread is running.
    assert(samples > 10000);

    std::cout << "test_wasapi_capture_integration PASSED" << std::endl;
}

int main() {
    std::cout << "=========================================" << std::endl;
    std::cout << "       LightRec Audio Unit Tests         " << std::endl;
    std::cout << "=========================================" << std::endl;

    test_ring_buffer();
    test_wasapi_capture_properties();
    test_wasapi_capture_integration();

    std::cout << "All audio tests completed successfully!" << std::endl;
    return 0;
}
